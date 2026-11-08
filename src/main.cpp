#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>
#include <format>

#include "imgui.h"
#include "platform.h"
#include "portaudio.h"

#ifdef __APPLE__
    #define SHARED_EXT ".dylib"
#else
    #define SHARED_EXT ".so"
#endif

static std::atomic<bool> quit{false};
static void on_sigint(int) { quit = true; }

static std::filesystem::file_time_type last_mtime = std::filesystem::file_time_type::min();

static std::mutex                 code_mutex;
static std::condition_variable    code_condition_variable;
static std::optional<std::string> code;

// mutex and stuff for log
static std::mutex               logging_text_mutex;
static std::vector<std::string> logging_text;

static void push_log_text(std::string msg)
{
    // lock so no other thread changes it
    std::lock_guard<std::mutex> log_text_lock(logging_text_mutex);

    // add the stuff to the log text
    logging_text.push_back(msg);

    // make it circular well techinically no but
    // it should delete old ones
    if (logging_text.size() > 100)
        logging_text.erase(logging_text.begin());
}

using PluginFunc = float (*)(float);

struct Plugin
{
    PluginFunc function = nullptr; // dsp function
    void*      handle   = nullptr; // handle to library dll or whatever

    bool valid() const { return function != nullptr; }

    ~Plugin()
    {
        if (handle) dlclose(handle);
    }
};

static Plugin* current_plugin = nullptr; // audio thread only, no atomic
static std::atomic<Plugin*> pending_plugin{nullptr};
static std::atomic<Plugin*> to_be_deleted_plugin{nullptr};

static int callback(const void* input, void* output, unsigned long frames,
                    const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void*)
{

    // if we have no plugin to be deleted
    if (to_be_deleted_plugin.load() == nullptr)
    {
        // get the pending plugin, and exchange it with nullptr
        Plugin* new_plugin = pending_plugin.exchange(nullptr);

        // if theres was actually something pending
        if (new_plugin)
        {
            // replace the current plugin with the new plugin
            Plugin* old_plugin = current_plugin;
            current_plugin = new_plugin;

            // tell the compiler to delete the old plugin
            if (old_plugin)
            {
                to_be_deleted_plugin.store(old_plugin);
            }
        }
    }

    const float* in = (const float*)input;
    float* out = (float*)output;

    // get the plugin
    Plugin* plugin = current_plugin;

    for (unsigned long i = 0; i < frames; i++)
    {
        float s = in ? in[i] : 0.0f;

        // if its even there
        if (plugin)
        {
            s = plugin->function(s);
        }

        out[i * 2] = out[i * 2 + 1] = s;
    }
    return paContinue;
}

// what gets added to the users plugin
// so they can focus on just the main func and stuff
static const char* PRELUDE =
    "#include <math.h>\n"
    "#include <stdlib.h>\n"
    "#include <stdio.h>\n"
    "#ifndef M_PI\n#define M_PI 3.14159265358979323846\n#endif\n"
    "float veksel_process_mono(float input);\n"
    "#define veksel_main float veksel_process_mono(float input)\n";

Plugin* compile_source(const std::string& source)
{
    // make tempory directory for compiler plugins
    char temp_dir_name[] = "/tmp/veksel_XXXXXX";

    // we should be able to make this
    if (!mkdtemp(temp_dir_name))
    {
        return nullptr;
    }

    std::filesystem::path temp_dir    = temp_dir_name;
    std::filesystem::path source_path = temp_dir / "plugin.c";
    std::filesystem::path dll_path    = temp_dir / "plugin.dylib";
    std::filesystem::path log_path    = temp_dir / "log.txt";

    // add the prelude to the source file
    std::ofstream(source_path) << PRELUDE << "#line 1\n" << source;

    // create compile command
    // vector so we can use posix in the future or something
    std::vector<std::string> _compile_cmd;

    // or whereever it is, might need to automate this
    _compile_cmd.push_back("/usr/bin/clang");
    _compile_cmd.push_back("-O3"); // OPTIMIzE!
    _compile_cmd.push_back("-march=native"); // machine specifc might not want this
    _compile_cmd.push_back("-ffast-math");
    _compile_cmd.push_back("-shared");
    _compile_cmd.push_back("-fPIC");
    #ifdef __APPLE__
        _compile_cmd.push_back("-undefined");
        _compile_cmd.push_back("dynamic_lookup");
    #endif
    _compile_cmd.push_back(source_path.string());
    _compile_cmd.push_back("-o");
    _compile_cmd.push_back(dll_path.string());


    std::string compile_cmd;
    for (const auto& arg : _compile_cmd)
    {
        compile_cmd += " ";
        compile_cmd += arg;
    }
    compile_cmd += " > " + log_path.string() + " 2>&1";


    if (system(compile_cmd.c_str()) != 0)
    {
        // compile failed
        // tell the logger
        std::ifstream log(log_path);
        std::string line;

        // print evech line to the log
        while (std::getline(log, line))
        {
            push_log_text(line);
        }

        push_log_text("[error] compile failed!");

        // remove the temp directory
        std::filesystem::remove_all(temp_dir);
        return nullptr;
    }

    // compile succeeded load the plugin
    void* handle = dlopen(dll_path.c_str(), RTLD_NOW);
    if (!handle)
    {
        const char* err = dlerror();
        push_log_text(std::format("dlopen: {}\n", err ? err : "(null)"));

        // remove the temp directory
        std::filesystem::remove_all(temp_dir);
        return nullptr;
    }

    // find the dsp function in the machine code
    PluginFunc function = (PluginFunc)dlsym(handle, "veksel_process_mono");
    if (!function)
    {
        // couldnt find it
        push_log_text("dlsym: no veksel_process_mono\n");
        dlclose(handle);
        std::filesystem::remove_all(temp_dir);
        return nullptr;
    }

    // safe to delete the temp dir
    // the function is mapped into memory
    std::filesystem::remove_all(temp_dir);

    // build and return the plugin
    auto* p = new Plugin();
    p->function = function;
    p->handle = handle;
    return p;
}

static void compiler_thread()
{

    while(!quit)
    {
        // get the pending plugin, and exchange it with nullptr
        delete to_be_deleted_plugin.exchange(nullptr);

        std::optional<std::string> source;
        {
            // use uniquelock so we can release mutex
            // if we we cant get the code yet
            std::unique_lock<std::mutex> lock(code_mutex);

            static auto wait_condition = [] { return code.has_value() || quit; };

            // wait for code to not be empty
            // i.e wait for some new code to get added that needs compiling
            code_condition_variable.wait_for(lock, std::chrono::milliseconds(10), wait_condition);
            if (quit)
            {
                return;
            }

            // source gets the value, code is now empty
            source = std::move(code);

            // nullify that code holder so we can add more later
            code.reset();
        } // lock released

        if (source)
        {
            // if we have some source to compile
            // compile it and get the new plugin
            Plugin* new_plugin = compile_source(*source);

            // if compiled
            if (new_plugin)
            {
                // update the pending plugin so we
                // can swap it in the audio thread
                Plugin* old_plugin = pending_plugin.exchange(new_plugin);

                // whatever was in the pending_plugin slot to begin with is old news
                // delete it
                delete old_plugin;

                push_log_text("[ok] compiled and swapped in");
            }
            else
            {
                // compile error or something
                // reduendant dont need this push_log_text("bad compile\n");
                push_log_text("[error] keeping current plugin");
            }
        }
    }
}

// use "stat" and global time var to check if file changed
static bool file_changed(const std::string& path)
{
    auto t = std::filesystem::last_write_time(path);
    if (t != last_mtime)
    {
        last_mtime = t;
        return true;
    }
    return false;
}

static std::string read_file_into_string(const std::string &path)
{
    std::ifstream f(path);

    if (!f.is_open())
    {
        push_log_text(std::format("could not open {}\n", path));
        return "";
    }

    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static char g_source[65536] = "veksel_main { return input; }";

static void draw_ui()
{

    ImGui::SeparatorText("Editor");
    ImGui::InputTextMultiline("##source", g_source, sizeof g_source,
                              ImVec2(-1, 300),
                              ImGuiInputTextFlags_AllowTabInput);

    if (ImGui::Button("Compile", ImVec2(-1, 0)))
    {
        {
            std::lock_guard<std::mutex> lock(code_mutex);
            code = std::string(g_source);
        }
        code_condition_variable.notify_one();   // notified after release
    }

    ImGui::SeparatorText("Log");
    ImGui::BeginChild("##log", ImVec2(-1, 150), ImGuiChildFlags_Borders);
    {
        std::lock_guard<std::mutex> log_text_lock(logging_text_mutex);
        for (auto& line : logging_text)
        {
            ImGui::TextUnformatted(line.c_str());
        }

        // stick to bottom if already at bottom
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f)
            ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
}

int main()
{
    std::signal(SIGINT, on_sigint);

    if (!platform_init("Veksel", 800, 900))
    {
        return 1;
    }

    if (Pa_Initialize() != paNoError)
    {
        return 1;
    }

    PaStreamParameters in_params = {};
    in_params.device = Pa_GetDefaultInputDevice();
    in_params.channelCount = 1;
    in_params.sampleFormat = paFloat32;
    in_params.suggestedLatency = 0.01;   // 10 ms, not the default

    PaStreamParameters out_params = {};
    out_params.device = Pa_GetDefaultOutputDevice();
    out_params.channelCount = 2;
    out_params.sampleFormat = paFloat32;
    out_params.suggestedLatency = 0.01;

    PaStream* stream;
    if (Pa_OpenStream(&stream, &in_params, &out_params, 44100, 64,
                  paClipOff, callback, nullptr) != paNoError)
    {
        return 1;
    }

    const PaStreamInfo* info = Pa_GetStreamInfo(stream);
    if (info)
    {
        //printf("Input latency:  %.1f ms\n", info->inputLatency * 1000.0);
        //printf("Output latency: %.1f ms\n", info->outputLatency * 1000.0);
        //printf("Sample rate:    %.0f Hz\n",  info->sampleRate);
    }

    // start compiler thread
    std::thread _compiler_thread(compiler_thread);

    const std::string plugin_file = "plugin.c";

    // create if missing
    if (!std::filesystem::exists(plugin_file))
    {
        std::ofstream(plugin_file) << "veksel_main { return input; }\n";
    }

    // do initial compile
    {
        std::lock_guard<std::mutex> lock(code_mutex);
        code = read_file_into_string(plugin_file);
    }
    code_condition_variable.notify_one();



    if (Pa_StartStream(stream) != paNoError)
    {
        return 1;
    }
    //printf("running. ctrl-c to quit.\n");

    while (platform_begin_frame())
    {
        // check if file change
        if( file_changed(plugin_file))
        {
            push_log_text("[*] file_changed\n");
            // read file and compile
            std::string new_code = read_file_into_string(plugin_file);

            {
                // get lock so compiler cant use it whilst we edit
                std::lock_guard<std::mutex> lock_code(code_mutex);

                // update the code
                code = new_code;
            } // use braces to automatically release mutex
            // tell thread we changed
            code_condition_variable.notify_one();
        }

        draw_ui();
        //Pa_Sleep(50);
        platform_end_frame();
    }

    quit = true;
    code_condition_variable.notify_one();
    _compiler_thread.join();
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    platform_shutdown();
}
