#include "VekselEngine.h"
#include "libtcc.h"

#include <cmath>
#include <ctime>
#include <cstring>
#include <sstream>
#include <string>
#include <utility>

using namespace veksel;

namespace
{
    const char* PRELUDE =
        "#include <math.h>\n"
        "#include <stdlib.h>\n"
        "#include <stdio.h>\n"
        "#ifndef M_PI\n"
        "#define M_PI 3.14159265358979323846\n"
        "#endif\n"
        "float veksel_process_mono(float input);\n"
        "#define veksel_main float veksel_process_mono(float input)\n";

    struct TccErrorBuffer
    {
        std::string text;
    };

    void tcc_error_callback(void* opaque, const char* msg)
    {
        auto* buf = static_cast<TccErrorBuffer*>(opaque);
        if (!buf || !msg) return;

        std::string m = msg;
        const std::string needle = "<string>:";
        std::string::size_type pos;
        while ((pos = m.find(needle)) != std::string::npos)
            m.replace(pos, needle.size(), "line ");

        buf->text += m;
        buf->text += '\n';
    }
}

// plugin destructor, deletes the TCCState, which frees the JIT-compiled code
Engine::Plugin::~Plugin()
{
    cleanup();
}

// clean up everything owned by this plugin
void Engine::Plugin::cleanup() noexcept
{
    if (tccState)
    {
        tcc_delete(tccState);
        tccState = nullptr;
    }
    function = nullptr;
}

// move constructor
// transfer ownership of the plugin instead of copying it
Engine::Plugin::Plugin(Plugin&& other) noexcept
    : function(std::exchange(other.function, nullptr))
    , tccState(std::exchange(other.tccState, nullptr))
{
}

// move assignment
// clean up anything we currently own before taking ownership of the new plugin
Engine::Plugin& Engine::Plugin::operator=(Plugin&& other) noexcept
{
    if (this != &other)
    {
        cleanup();
        function = std::exchange(other.function, nullptr);
        tccState = std::exchange(other.tccState, nullptr);
    }
    return *this;
}

// stop the compiler thread when the engine is destroyed.
// the caller must stop the audio stream before destroying the engine,
// so processSample() cannot run against a destroyed object.
Engine::~Engine()
{
    stop();
}

// process a single audio sample
// this gets called by the audio callback
float Engine::processSample(float input)
{
    // if a plugin swap has been requested
    // tell the ui thread we are no longer using the old plugin
    if (m_swapRequested.load(std::memory_order_acquire))
    {
        m_swapAcknowledged.store(true, std::memory_order_release);
        return input;
    }

    // if no plugin is loaded just pass the input through
    if (!m_activePlugin.function)
        return input;

    // run the sample through the active plugin
    return m_activePlugin.function(input);
}

// check whether a newly compiled plugin is ready to be installed
void Engine::update()
{
    // protect the pending plugin while checking or moving it
    // the compiler uses the same mutex when publishing a new plugin.
    std::lock_guard<std::mutex> lock(m_pluginMutex);

    // nothing to install.
    if (!m_pendingPlugin)
        return;

    // if a handover hasn't started yet, request one.
    if (!m_swapRequested.load(std::memory_order_acquire))
    {
        writeLog("[sys] New plugin ready. Waiting for audio thread.");

        // clear any previous acknowledgement before requesting the swap.
        m_swapAcknowledged.store(false, std::memory_order_relaxed);

        // tell the audio thread to stop using the active plugin
        // and acknowledge when it reaches a safe point.
        m_swapRequested.store(true, std::memory_order_release);

        // check for the acknowledgement on the next UI frame.
        return;
    }

    // the audio thread hasn't reached a safe point yet.
    // keep the pending plugin intact and check again next frame.
    if (!m_swapAcknowledged.load(std::memory_order_acquire))
        return;

    writeLog("[sys] Audio thread acknowledged plugin swap.");

    // install the newly compiled plugin.
    // move assignment destroys the old plugin and its TCC state,
    // which is now safe because the audio thread acknowledged.
    m_activePlugin = std::move(m_pendingPlugin);

    // allow the audio thread to start using the new plugin
    m_swapRequested.store(false, std::memory_order_release);

    writeLog("[ok] Plugin successfully loaded.");
}

// compile the user's C source code into executable memory
Engine::Plugin Engine::compileSource(const std::string& source)
{
    writeLog("[sys] Starting TCC compilation.");
    writeLog("[sys] Source size: " + std::to_string(source.size()) + " bytes.");

    // create a new tiny cc state
    TCCState* state = tcc_new();
    if (!state)
    {
        writeLog("[error] tcc_new() failed.");
        return {};
    }

    TccErrorBuffer errors;
    tcc_set_error_func(state, &errors, tcc_error_callback);

    tcc_set_lib_path(state, VEKSEL_TCC_LIB_PATH);

    // memory output - no files written
    tcc_set_output_type(state, TCC_OUTPUT_MEMORY);

    // compile prelude + user code
    std::string fullSource = std::string(PRELUDE) + "\n#line 1\n" + source;

    // compile the file
    if (tcc_compile_string(state, fullSource.c_str()) == -1)
    {
        writeLog("[error] Compilation failed.");

        // split errors by newline and log each line separately
        std::stringstream ss(errors.text);
        std::string line;
        while (std::getline(ss, line)) {
            if (!line.empty()) writeLog("[error] " + line);
        }

        tcc_delete(state);
        return {};
    }

    // register the math functions exposed to user code.
    // this ensures these functions can be resolved during relocation.
    auto add = [&](const char* name, void* fn) { tcc_add_symbol(state, name, fn); };

    add("sinf",   reinterpret_cast<void*>(sinf));
    add("cosf",   reinterpret_cast<void*>(cosf));
    add("tanf",   reinterpret_cast<void*>(tanf));
    add("fabsf",  reinterpret_cast<void*>(fabsf));
    add("powf",   reinterpret_cast<void*>(powf));
    add("expf",   reinterpret_cast<void*>(expf));
    add("logf",   reinterpret_cast<void*>(logf));
    add("log2f",  reinterpret_cast<void*>(log2f));
    add("log10f", reinterpret_cast<void*>(log10f));
    add("sqrtf",  reinterpret_cast<void*>(sqrtf));
    add("floorf", reinterpret_cast<void*>(floorf));
    add("ceilf",  reinterpret_cast<void*>(ceilf));
    add("fmodf",  reinterpret_cast<void*>(fmodf));
    add("atanf",  reinterpret_cast<void*>(atanf));
    add("atan2f", reinterpret_cast<void*>(atan2f));
    add("sinhf",  reinterpret_cast<void*>(sinhf));
    add("coshf",  reinterpret_cast<void*>(coshf));
    add("tanhf",  reinterpret_cast<void*>(tanhf));

    // relocate the plugin to the state
    // relocate failure
    if (tcc_relocate(state) < 0)
    {
        writeLog("[error] Relocation failed.");

        // split errors by newline and log each line separately
        std::stringstream ss(errors.text);
        std::string line;
        while (std::getline(ss, line)) {
            if (!line.empty()) writeLog("[error] " + line);
        }

        tcc_delete(state);
        return {};
    }

    // resolve the entry point
    auto func = reinterpret_cast<ProcessFunc>(
        tcc_get_symbol(state, "veksel_process_mono")
    );

    if (!func)
    {
        writeLog("[error] Symbol 'veksel_process_mono' not found.");
        tcc_delete(state);
        return {};
    }

    writeLog("[ok] Compilation successful.");

    Plugin plugin;
    plugin.function = func;
    plugin.tccState = state;
    return plugin;
}

// background thread responsible for compiling submitted source code
void Engine::compilerLoop()
{
    while (m_running)
    {
        std::string source;

        // wait until new source code is submitted
        {
            std::unique_lock<std::mutex> lock(m_sourceMutex);

            m_sourceConditionVariable.wait(lock, [this]
            {
                return !m_running || m_hasPendingSource;
            });

            // stop if the engine is shutting down
            if (!m_running)
                break;

            // take the submitted source and clear the pending slot
            source = std::move(m_pendingSource);
            m_hasPendingSource = false;
        }

        // compile outside the mutex so the ui can submit more code
        Plugin plugin = compileSource(source);

        // compilation failed, keep using the current plugin
        if (!plugin)
            continue;

        // don't publish a plugin if the engine is stopping
        if (!m_running)
            break;

        // publish the newly compiled plugin for the ui thread to pick up
        {
            std::lock_guard<std::mutex> lock(m_pluginMutex);
            m_pendingPlugin = std::move(plugin);
        }
    }
}

// start the background compiler thread
void Engine::start()
{
    // don't start another thread if we're already running
    if (m_running.load())
        return;

    // join the previous thread if it has already finished
    if (m_compilerThread.joinable())
        m_compilerThread.join();

    m_running.store(true);

    // start the compiler thread
    m_compilerThread = std::thread(&Engine::compilerLoop, this);
}

// stop the background compiler thread
void Engine::stop()
{
    {
        // tell the compiler thread to stop
        std::lock_guard<std::mutex> lock(m_sourceMutex);
        m_running.store(false);
    }

    // wake the thread in case it is waiting for source code
    m_sourceConditionVariable.notify_one();

    // wait for the compiler thread to finish
    if (m_compilerThread.joinable())
        m_compilerThread.join();
}

// submit new source code for compilation
// the compiler thread will pick it up in the background
void Engine::submitSource(const std::string& source)
{
    {
        // update the pending source under the mutex
        std::lock_guard<std::mutex> lock(m_sourceMutex);

        m_pendingSource = source;
        m_hasPendingSource = true;
    }

    writeLog("[sys] Compilation requested (" +
              std::to_string(source.size()) + " bytes).");

    // wake the compiler thread
    m_sourceConditionVariable.notify_one();
}

// add a message to the engine log, prefixed with the current time
void Engine::writeLog(const std::string& message)
{
    const std::time_t t = std::time(nullptr);
    std::tm tm{};

    #if defined(_WIN32)
        localtime_s(&tm, &t);
    #else
        localtime_r(&t, &tm);
    #endif

    char stamp[16];
    std::strftime(stamp, sizeof stamp, "%H:%M:%S ", &tm);

    std::lock_guard<std::mutex> lock(m_logMutex);
    m_log.push_back(stamp + message);

    // cap the log at 100 lines, dropping the oldest 50 when full
    if (m_log.size() > 100)
    {
        m_log.erase(m_log.begin(), m_log.begin() + 50);
    }
}
// return a copy of the engine log
std::vector<std::string> Engine::getLog() const
{
    std::lock_guard<std::mutex> lock(m_logMutex);
    return m_log;
}

// clear all messages from the engine log
void Engine::clearLog()
{
    std::lock_guard<std::mutex> lock(m_logMutex);
    m_log.clear();
}
