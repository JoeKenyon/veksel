
#pragma once

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

// forward-declare so we don't pull libtcc.h
struct TCCState;

namespace veksel
{
    class Engine
    {
    public:

        // Create the engine with default
        Engine() = default;

        // Stop the compiler thread and clean up when destroyed
        ~Engine();

        // Prevent copying the engine
        // There should only be one owner of its threads and plugins
        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;

        // Start the background compiler thread
        void start();
        // Stop the background compiler thread
        void stop();

        // Submit source code to be compiled in the background
        void submitSource(const std::string& source);

        // Check for completed compilation and swap plugins
        // Call from the UI/control thread
        void update();

        // Process one audio sample
        // Call from the audio thread
        float processSample(float input);

        // Return a copy of the engine log
        std::vector<std::string> getLog() const;

        // Clear all messages from the log
        void clearLog();

    private:
        // Function signature expected from a compiled plugin
        using ProcessFunc = float (*)(float);

        // Represents a compiled plugin and owns its resources
        struct Plugin
        {
            ProcessFunc function = nullptr;
            TCCState* tccState = nullptr;

            // Automatically clean up the plugin when destroyed
            ~Plugin();

            Plugin() = default;

            // Plugins own resources, so they cannot be copied
            Plugin(const Plugin&) = delete;
            Plugin& operator=(const Plugin&) = delete;

            // Transfer ownership when moving a plugin
            Plugin(Plugin&& other) noexcept;
            Plugin& operator=(Plugin&& other) noexcept;

            // Check whether the plugin was loaded successfully
            explicit operator bool() const { return function != nullptr; }

        private:
            // Unload the library and remove its temporary directory
            void cleanup() noexcept;
        };

        // Wait for submitted source code and compile it
        void compilerLoop();

        // Compile source code and return the loaded plugin
        Plugin compileSource(const std::string& source);

        // Add a message to the engine log
        void writeLog(const std::string& message);

        // Background compiler thread
        std::atomic<bool> m_running = false;
        std::thread m_compilerThread;

        // Source code waiting to be compiled
        std::mutex m_sourceMutex;
        std::condition_variable m_sourceConditionVariable;
        std::string m_pendingSource;
        bool m_hasPendingSource = false;

        // Newly compiled plugin waiting to be installed
        std::mutex m_pluginMutex;
        Plugin m_pendingPlugin;

        // Plugin currently being used by the audio thread
        Plugin m_activePlugin;

        // Used to safely swap plugins between the UI and audio threads
        std::atomic<bool> m_swapRequested = false;
        std::atomic<bool> m_swapAcknowledged = false;

        // Engine log
        mutable std::mutex m_logMutex;
        std::vector<std::string> m_log;
    };
}
