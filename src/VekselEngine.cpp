
#include "VekselEngine.h"

#ifdef _WIN32
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

#include <random>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>
#include <stdlib.h>
#include <ctime>
#include <unistd.h>

namespace
{
    // load the compiled plugin into memory
    // windows uses LoadLibrary, mac and linux use dlopen
    void* load_library(const std::filesystem::path& path)
    {
    #ifdef _WIN32
        return LoadLibraryW(path.c_str());
    #else
        return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    #endif
    }

    // unload the plugin from memory
    // this should only happen when the audio thread is no longer using it
    void unload_library(void* handle)
    {
        if (!handle)
            return;

    #ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(handle));
    #else
        dlclose(handle);
    #endif
    }

    // find a function inside the compiled plugin
    // this is how we get access to the dsp function
    void* load_function(void* handle, const char* name)
    {
    #ifdef _WIN32
        return reinterpret_cast<void*>(
            GetProcAddress(static_cast<HMODULE>(handle), name)
        );
    #else
        return dlsym(handle, name);
    #endif
    }

    // get the correct shared library extension for the current platform
    std::string library_extension()
    {
    #ifdef _WIN32
        return ".dll";
    #elif __APPLE__
        return ".dylib";
    #else
        return ".so";
    #endif
    }

    // what gets added to the users plugin
    // so they can focus on just writing their dsp code
    const char* PRELUDE =
        "#include <math.h>\n"
        "#include <stdlib.h>\n"
        "#include <stdio.h>\n"
        "#ifndef M_PI\n"
        "#define M_PI 3.14159265358979323846\n"
        "#endif\n"
        "float veksel_process_mono(float input);\n"
        "#define veksel_main float veksel_process_mono(float input)\n";

    // wrap strings in quotes so paths with spaces work in the compiler command
    // this uses posix shell quoting
    static std::string shell_quote(const std::string& value)
    {
        std::string result = "'";
        for (char c : value)
        {
            if (c == '\'')
                result += "'\\''";
            else
                result += c;
        }
        result += "'";
        return result;
    }

    // make a temporary directory for the compiled plugin
    // use the systems temp directory instead of assuming a specific path
    std::filesystem::path create_temp_directory()
    {
        namespace fs = std::filesystem;

        static std::mt19937_64 generator(std::random_device{}());

        // try a few random directory names in case one already exists
        for (int i = 0; i < 100; ++i)
        {
            fs::path directory =
                fs::temp_directory_path() /
                ("veksel-" + std::to_string(generator()));

            std::error_code error;

            // create_directory only succeeds if the directory doesn't already exist
            if (fs::create_directory(directory, error))
                return directory;

            if (error && error != std::errc::file_exists)
                break;
        }

        return {};
    }
}

// plugin destructor
// automatically unloads the library and removes its temporary directory
veksel::Engine::Plugin::~Plugin()
{
    cleanup();
}

// clean up everything owned by this plugin
void veksel::Engine::Plugin::cleanup() noexcept
{
    // unload the library first
    if (handle)
    {
        unload_library(handle);
        handle = nullptr;
    }

    // remove the temporary directory and its files
    if (!directory.empty())
    {
        std::error_code error;
        std::filesystem::remove_all(directory, error);
        directory.clear();
    }

    function = nullptr;
}

// move constructor
// transfer ownership of the plugin instead of copying it
veksel::Engine::Plugin::Plugin(Plugin&& other) noexcept
{
    function = std::exchange(other.function, nullptr);
    handle = std::exchange(other.handle, nullptr);
    directory = std::move(other.directory);
}

// move assignment
// clean up anything we currently own before taking ownership of the new plugin
veksel::Engine::Plugin& veksel::Engine::Plugin::operator=(Plugin&& other) noexcept
{
    if (this != &other)
    {
        cleanup();

        function = std::exchange(other.function, nullptr);
        handle = std::exchange(other.handle, nullptr);
        directory = std::move(other.directory);
    }

    return *this;
}

// create the engine with the supplied settings
veksel::Engine::Engine(Settings settings)
    : settings(std::move(settings))
{
}

// default constructor uses the default settings
veksel::Engine::Engine()
    : Engine(Settings{})
{
}

// stop the compiler thread when the engine is destroyed
veksel::Engine::~Engine()
{
    stop();
}

// process a single audio sample
// this gets called by the audio callback
float veksel::Engine::process_sample(float input)
{
    // if a plugin swap has been requested
    // tell the ui thread we are no longer using the old plugin
    if (swap_requested.load(std::memory_order_acquire))
    {
        swap_acknowledged.store(true, std::memory_order_release);
        return input;
    }

    // if no plugin is loaded just pass the input through
    if (!active_plugin.function)
        return input;

    // run the sample through the active plugin
    return active_plugin.function(input);
}

// check if there is a new plugin ready to be swapped in
// this gets called from the ui thread
void veksel::Engine::update()
{
    Plugin new_plugin;

    {
        // get the newly compiled plugin
        std::lock_guard<std::mutex> lock(plugin_mutex);

        if (!pending_plugin)
            return;

        new_plugin = std::move(pending_plugin);
    }

    write_log("[sys] New plugin ready. Waiting for audio thread.");

    // ask the audio thread to stop using the current plugin
    swap_acknowledged.store(false, std::memory_order_relaxed);
    swap_requested.store(true, std::memory_order_release);

    // wait until the audio thread acknowledges the request
    while (!swap_acknowledged.load(std::memory_order_acquire))
        std::this_thread::yield();

    write_log("[sys] Audio thread acknowledged plugin swap.");

    // replace the old plugin with the new one
    // move assignment automatically cleans up the old plugin
    active_plugin = std::move(new_plugin);

    write_log("[ok] Plugin successfully loaded.");

    // let the audio thread start processing again
    swap_requested.store(false, std::memory_order_release);
}

// compile the users source code into a shared library
veksel::Engine::Plugin veksel::Engine::compile_source(const std::string& source)
{
    write_log("[sys] Starting compilation.");
    write_log("[sys] Source size: " + std::to_string(source.size()) + " bytes.");
    write_log("[sys] Compiler: " + settings.compiler);

    namespace fs = std::filesystem;

    // create a temporary directory for this compilation
    fs::path dir = create_temp_directory();

    if (dir.empty())
    {
        write_log("[error] Failed to create temporary directory.");
        return {};
    }

    // paths for the source file, compiled plugin and compiler output
    fs::path source_path = dir / "plugin.c";
    fs::path library_path = dir / ("plugin" + library_extension());
    fs::path log_path = dir / "compiler.log";

    // write the prelude and users source code into the file
    // #line makes compiler errors refer to the users code
    {
        std::ofstream file(source_path);

        if (!file)
        {
            write_log("[error] Failed to write source file.");
            fs::remove_all(dir);
            return {};
        }

        file << PRELUDE << "\n#line 1\n" << source;
    }

    // build the compiler command
    // use the compiler specified in the engine settings
    std::string command =
        shell_quote(settings.compiler) +
        " -O2"
        " -dynamiclib"
        " -fPIC"
        " " + shell_quote(source_path.string()) +
        " -o " + shell_quote(library_path.string()) +
        " > " + shell_quote(log_path.string()) +
        " 2>&1";

    write_log("[sys] Running compiler.");
    // run the compiler
    int result = std::system(command.c_str());

    // read the compiler output and add it to the engine log
    {
        std::ifstream file(log_path);
        std::stringstream output;

        output << file.rdbuf();

        if (!output.str().empty())
            write_log(output.str());
    }

    // if compilation failed, clean up and keep the current plugin
    if (result != 0)
    {
        write_log("[error] Compiler exited with status " + std::to_string(result) + ".");
        write_log("[error] Compilation failed.");
        fs::remove_all(dir);
        return {};
    }

    write_log("[ok] Compilation completed.");

    // load the compiled plugin into memory
    void* handle = load_library(library_path);

    if (!handle)
    {
        write_log("[error] Failed to load plugin.");
        fs::remove_all(dir);
        return {};
    }

    // find the dsp function inside the compiled library
    auto function = reinterpret_cast<ProcessFunc>(
        load_function(handle, "veksel_process_mono")
    );

    // if the function doesn't exist, unload the library and clean up
    if (!function)
    {
        write_log("[error] Failed to find plugin function.");

        unload_library(handle);
        fs::remove_all(dir);
        return {};
    }

    write_log("[ok] Compilation successful.");

    // build the plugin object
    // it now owns the library handle and temporary directory
    Plugin plugin;
    plugin.function = function;
    plugin.handle = handle;
    plugin.directory = std::move(dir);

    return plugin;
}

// background thread responsible for compiling submitted source code
void veksel::Engine::compiler_loop()
{
    while (running)
    {
        std::string source;

        // wait until new source code is submitted
        {
            std::unique_lock<std::mutex> lock(source_mutex);

            source_condition_variable.wait(lock, [this]
            {
                return !running || has_pending_source;
            });

            // stop if the engine is shutting down
            if (!running)
                break;

            // take the submitted source and clear the pending slot
            source = std::move(pending_source);
            has_pending_source = false;
        }

        // compile outside the mutex so the ui can submit more code
        Plugin plugin = compile_source(source);

        // compilation failed, keep using the current plugin
        if (!plugin)
            continue;

        // don't publish a plugin if the engine is stopping
        if (!running)
            break;

        // publish the newly compiled plugin for the ui thread to pick up
        {
            std::lock_guard<std::mutex> lock(plugin_mutex);
            pending_plugin = std::move(plugin);
        }
    }
}

// start the background compiler thread
void veksel::Engine::start()
{
    // don't start another thread if we're already running
    if (running.load())
        return;

    // join the previous thread if it has already finished
    if (compiler_thread.joinable())
        compiler_thread.join();

    running.store(true);

    // start the compiler thread
    compiler_thread = std::thread(&Engine::compiler_loop, this);
}

// stop the background compiler thread
void veksel::Engine::stop()
{
    {
        // tell the compiler thread to stop
        std::lock_guard<std::mutex> lock(source_mutex);
        running.store(false);
    }

    // wake the thread in case it is waiting for source code
    source_condition_variable.notify_one();

    // wait for the compiler thread to finish
    if (compiler_thread.joinable())
        compiler_thread.join();
}

// submit new source code for compilation
// the compiler thread will pick it up in the background
void veksel::Engine::submit_source(const std::string& source)
{
    {
        // update the pending source under the mutex
        std::lock_guard<std::mutex> lock(source_mutex);

        pending_source = source;
        has_pending_source = true;
    }

    write_log("[sys] Compilation requested (" +
              std::to_string(source.size()) + " bytes).");

    // wake the compiler thread
    source_condition_variable.notify_one();
}

// add a message to the engine log, prefixed with the current time
void veksel::Engine::write_log(const std::string& message)
{
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_r(&t, &tm); // thread safe, called from the compiler thread

    char stamp[16];
    std::strftime(stamp, sizeof stamp, "%H:%M:%S ", &tm);

    std::lock_guard<std::mutex> lock(log_mutex);
    log.push_back(stamp + message);
}
// return a copy of the engine log
std::vector<std::string> veksel::Engine::get_log() const
{
    std::lock_guard<std::mutex> lock(log_mutex);
    return log;
}

// clear all messages from the engine log
void veksel::Engine::clear_log()
{
    std::lock_guard<std::mutex> lock(log_mutex);
    log.clear();
}
