#include "Engine.h"

#include <libtcc.h>

#include <chrono>
#include <cmath>
#include <utility>

namespace veksel
{
    struct Engine::Plugin
    {
        using ProcessSampleFn = float (*)(float);

        // tcc owns the machine code + globals, so it has to stay alive until retired
        TCCState*       tcc           = nullptr;
        ProcessSampleFn processSample = nullptr;

        ~Plugin()
        {
            if (tcc) tcc_delete(tcc);
        }
    };

    constexpr std::string_view Prelude =
        "#include <math.h>\n"
        "#include <stdlib.h>\n"
        "#include <stdio.h>\n"
        "#ifndef M_PI\n"
        "#define M_PI 3.14159265358979323846\n"
        "#endif\n";

    static void tccErrorCallback(void* opaque, const char* message)
    {
        if (!opaque || !message)
            return;

        std::string text = message;

        // tcc calls the source "<string>:", show it as "line N:" instead
        constexpr std::string_view needle = "<string>:";
        for (auto pos = text.find(needle); pos != std::string::npos; pos = text.find(needle, pos))
            text.replace(pos, needle.size(), "line ");

        auto& errors = *static_cast<std::string*>(opaque);
        errors += text;
        errors += '\n';
    }

    Engine::Engine(Log& log)
        : m_log(log)
    {
        m_compilerThread = std::thread(&Engine::compilerThreadLoop, this);
    }

    Engine::~Engine()
    {
        {
            std::lock_guard<std::mutex> lock(m_compilerMutex);
            m_stopRequested = true;
        }
        m_compilerCv.notify_one();

        if (m_compilerThread.joinable())
            m_compilerThread.join();

        // safe to wipe everything now that both audio and compiler threads are stopped
        drainRetired();
        delete m_pendingPlugin.exchange(nullptr, std::memory_order_acquire);
        delete m_activePlugin;
        m_activePlugin = nullptr;
    }

    // audio thread: never locks or frees, just swaps pointers if a build is ready
    void Engine::beginBlock() noexcept
    {
        if (Plugin* next = m_pendingPlugin.exchange(nullptr, std::memory_order_acq_rel))
        {
            if (m_activePlugin)
                m_retiredQueue.push(m_activePlugin);

            m_activePlugin = next;
        }
    }

    float Engine::processSample(float input) noexcept
    {
        // passthrough before anything has compiled
        if (!m_activePlugin)
            return input;

        // no nans and stuff
        float out = m_activePlugin->processSample(input);
        return std::isfinite(out) ? out : 0.0f;
    }

    void Engine::submitSource(std::string source)
    {
        {
            std::lock_guard<std::mutex> lock(m_compilerMutex);
            m_pendingSource    = std::move(source);
            m_hasPendingSource = true;
            m_compiling.store(true, std::memory_order_relaxed);
        }
        m_compilerCv.notify_one();
    }

    void Engine::drainRetired()
    {
        Plugin* old = nullptr;
        while (m_retiredQueue.pop(old))
        {
            delete old;
        }
    }

    Engine::Plugin* Engine::compileWithTcc(const std::string& source)
    {
        std::string errorLog;
        TCCState* s = tcc_new();
        if (!s)
        {
            m_log.write("[error] Failed to create TCCState");
            return nullptr;
        }

        tcc_set_error_func(s, &errorLog, &tccErrorCallback);
        tcc_set_lib_path(s, VEKSEL_TCC_LIB_PATH); // make this portable
        tcc_set_output_type(s, TCC_OUTPUT_MEMORY);

        // give user code math.h out of the box without needing an explicit #include
        const std::string fullSource = std::string(Prelude).append("#line 1\n").append(source);

        if (tcc_compile_string(s, fullSource.c_str()) == -1 || tcc_relocate(s) < 0)
        {
            tcc_delete(s);
            m_log.write("[error] " + (errorLog.empty() ? std::string("Compilation failed") : errorLog));
            return nullptr;
        }

        auto fn = reinterpret_cast<Plugin::ProcessSampleFn>(tcc_get_symbol(s, "process_sample"));
        if (!fn)
        {
            tcc_delete(s);
            m_log.write("[error] Missing required function: float process_sample(float input)");
            return nullptr;
        }

        auto* plugin          = new Plugin();
        plugin->tcc           = s;
        plugin->processSample = fn;

        m_log.write("[ok] Compiled successfully");
        return plugin;
    }

    void Engine::compilerThreadLoop()
    {
        std::unique_lock<std::mutex> lock(m_compilerMutex);

        while (!m_stopRequested)
        {
            // poll every 100ms so retired plugins get freed even if nobody recompiles
            m_compilerCv.wait_for(lock, std::chrono::milliseconds(100), [this] 
            {
                return m_hasPendingSource || m_stopRequested;
            });

            drainRetired();

            if (m_hasPendingSource)
            {
                std::string source = std::move(m_pendingSource);
                m_hasPendingSource = false;
                lock.unlock();

                if (Plugin* next = compileWithTcc(source))
                {
                    // if audio never grabbed the previous build (e.g. stream stopped),
                    // exchange hands it straight back to us to delete
                    Plugin* unconsumed = m_pendingPlugin.exchange(next, std::memory_order_acq_rel);
                    delete unconsumed;
                }

                lock.lock();
                // stay "compiling" if another submitSource came in while tcc was running
                // button gets disabled so this shouldnt happen
                // but maybe it could if i load a plugin through file loading
                if (!m_hasPendingSource)
                    m_compiling.store(false, std::memory_order_relaxed);
            }
        }

        drainRetired();
    }
}