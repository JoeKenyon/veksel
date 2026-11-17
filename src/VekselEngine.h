#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct TCCState;

namespace veksel
{
    class Engine
    {
    public:
        // signature of the jit-compiled entry point
        using ProcessFunc = float (*)(float);

        struct Plugin
        {
            ProcessFunc function = nullptr;
            TCCState*   tccState = nullptr;

            Plugin() = default;
            ~Plugin();

            Plugin(Plugin&& other) noexcept;
            Plugin& operator=(Plugin&& other) noexcept;

            // ownership is unique: the tcc state and function pointer must not
            // be copied. the move constructor already deletes these,
            // but declaring them makes the ownership model explicit.
            Plugin(const Plugin&) = delete;
            Plugin& operator=(const Plugin&) = delete;

            // check whether the plugin was loaded successfully
            explicit operator bool() const noexcept { return function != nullptr; }

            // frees the tcc state (and therefore the compiled code)
            void cleanup() noexcept;
        };

        Engine() = default;
        ~Engine();

        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;

        // must only be called from one serial audio callback thread.
        // the plugin swap acknowledgement relies on sequential invocation.
        float processSample(float input);

        // called from the ui thread once per frame.
        // installs a pending plugin once the audio thread has acknowledged.
        void update();

        // start / stop the background compiler thread
        void start();
        void stop();

        // submit source for compilation on the background thread
        void submitSource(const std::string& source);

        // log access (ui thread)
        std::vector<std::string> getLog() const;
        void clearLog();
        void writeLog(const std::string& message);

    private:
        Plugin compileSource(const std::string& source);
        void   compilerLoop();

        // active plugin - read by the audio thread, written by the ui thread
        // only after m_swapAcknowledged is observed
        Plugin m_activePlugin;

        // newly compiled plugin waiting to be installed
        Plugin m_pendingPlugin;

        // two-atomic swap handshake
        std::atomic<bool> m_swapRequested{false};    // ui -> audio
        std::atomic<bool> m_swapAcknowledged{false}; // audio -> ui

        // pending plugin guard (compiler publishes, ui consumes)
        std::mutex m_pluginMutex;

        // compiler thread + source queue
        std::thread             m_compilerThread;
        std::atomic<bool>       m_running{false};
        std::mutex              m_sourceMutex;
        std::condition_variable m_sourceConditionVariable;
        std::string             m_pendingSource;
        bool                    m_hasPendingSource = false;

        // log
        mutable std::mutex       m_logMutex;
        std::vector<std::string> m_log;
    };
}
