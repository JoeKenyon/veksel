#include "Compiler.h"
#include "AudioEngine.h"
#include "Plugin.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>

#ifdef __APPLE__
    #define SHARED_EXT ".dylib"
#else
    #define SHARED_EXT ".so"
#endif

namespace fs = std::filesystem;
extern char **environ;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static std::string make_prelude()
{
    std::string s =
        "#include <math.h>\n"
        "#include <stdlib.h>\n"
        "#include <stdio.h>\n"
        "#ifndef M_PI\n#define M_PI 3.14159265358979323846\n#endif\n"
        "extern float veksel_get_knob(int);\n"
        "extern int veksel_get_switch(int);\n"
        "extern double veksel_get_sample_rate();\n"
        "#define SAMPLE_RATE veksel_get_sample_rate()\n"
        "typedef struct { float left, right; } Stereo;\n"
        "float  veksel_process_mono(float input);\n"
        "Stereo veksel_process_stereo(Stereo input);\n"
        "#define veksel_main float veksel_process_mono(float input)\n"
        "#define veksel_main_stereo Stereo veksel_process_stereo(Stereo input)\n";

    for (int i = 1; i <= 12; i++)
    {
        s += "#define KNOB_" + std::to_string(i) + " veksel_get_knob(" + std::to_string(i - 1) + ")\n";
    }

    for (int i = 1; i <= 3; i++)
    {
        s += "#define SWITCH_" + std::to_string(i) + " veksel_get_switch(" + std::to_string(i - 1) + ")\n";
    }

    return s;
}

// mmmm slurpy
static std::string slurp(const fs::path& p)
{
    std::ifstream f(p);
    return std::string((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
}


static int run_process(const std::vector<std::string>& args,
                       int timeout_ms,
                       const fs::path& log)
{
    std::vector<char*> argv;
    for (const auto& a : args)
    {
        argv.push_back(const_cast<char*>(a.c_str()));
    }
    argv.push_back(nullptr);

    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    if (!log.empty())
    {
        posix_spawn_file_actions_addopen(&fa, 1, log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        posix_spawn_file_actions_adddup2(&fa, 1, 2);
    }

    pid_t pid;
    int rc = posix_spawnp(&pid, argv[0], &fa, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&fa);
    if (rc != 0)
    {
        return -1;
    }

    int status = 0;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (waitpid(pid, &status, WNOHANG) == pid)
        {
            return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    kill(pid, SIGKILL);
    waitpid(pid, &status, 0);
    return -1;
}

// ---------------------------------------------------------------------------
// Compiler
// ---------------------------------------------------------------------------
Compiler::Compiler(AudioEngine& engine) : engine_(engine)
{

}

Compiler::~Compiler()
{
    stop();
}

void Compiler::start()
{
    thread_ = std::thread([this]
        {
            loop();
        }
    );
}

void Compiler::submit(std::string source)
{
    {
        std::lock_guard lk(mu_);
        code_ = std::move(source);
    }
    cv_.notify_one();
}

void Compiler::stop()
{
    {
        std::lock_guard lk(mu_);
        quit_ = true;
    }
    cv_.notify_one();
    if (thread_.joinable())
    {
        thread_.join();
    }
}

void Compiler::report(const std::string& msg)
{
    if (!quit_)
    {
        printf("%s\n", msg.c_str());
    }
}

PluginPtr Compiler::build(const std::string& source)
{
    char tmpl[] = "/tmp/veksel_XXXXXX";
    if (!mkdtemp(tmpl))
    {
        report("could not create temp dir");
        return nullptr;
    }

    fs::path dir = tmpl;
    fs::path src = dir / "plugin.c";
    fs::path lib = dir / ("plugin" SHARED_EXT);
    fs::path log = dir / "log.txt";

    std::ofstream(src) << make_prelude() << "#line 1\n" << source;

    std::vector<std::string> cc = {"/usr/bin/clang", "-O1", "-shared", "-fPIC"};
#ifdef __APPLE__
    cc.insert(cc.end(), {"-undefined", "dynamic_lookup"});
#endif
    cc.push_back(src.string());
    cc.push_back("-o");
    cc.push_back(lib.string());

    // compile
    if (run_process(cc, 30000, log) != 0)
    {
        report("compile failed\n" + slurp(log));
        std::error_code ec;
        fs::remove_all(dir, ec);
        return nullptr;
    }

    // dlopen BEFORE cleanup the file must exist on disk when dlopen runs
    auto p = load_plugin(lib.c_str());

    // now safe to delete the temp dir: the .dylib is mapped into memory
    std::error_code ec;
    fs::remove_all(dir, ec);

    if (!p)
    {
        report("plugin has neither veksel_main nor veksel_main_stereo");
    }
    return p;
}

void Compiler::loop()
{
    while (true)
    {
        engine_.reclaim();

        std::optional<std::string> code;
        {
            std::unique_lock lk(mu_);
            cv_.wait_for(lk, std::chrono::milliseconds(10),
                         [&] { return code_.has_value() || quit_; });
            if (quit_) return;
            code.swap(code_);
        }

        if (!code)
        {
            continue;
        }

        auto p = build(*code);
        if (!p)
        {
            continue;
        }

        if (quit_)
        {
            return;
        }

        engine_.publish(std::move(p));
        report("plugin reloaded");
    }
}
