
#pragma once

#include <string>
#include <vector>
#include <thread>
#include <filesystem>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace veksel
{
    class Engine
    {
    public:
        // Settings used when compiling plugins
        struct Settings
        {
            std::string compiler = "clang";
        };

        // Create the engine with default or custom settings
        Engine();
        explicit Engine(Settings settings);

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
        void submit_source(const std::string& source);

        // Check for completed compilation and swap plugins
        // Call from the UI/control thread
        void update();

        // Process one audio sample
        // Call from the audio thread
        float process_sample(float input);

        // Return a copy of the engine log
        std::vector<std::string> get_log() const;

        // Clear all messages from the log
        void clear_log();

    private:
        // Function signature expected from a compiled plugin
        using ProcessFunc = float (*)(float);

        // Represents a compiled plugin and owns its resources
        struct Plugin
        {
            ProcessFunc function = nullptr;
            void* handle = nullptr;
            std::filesystem::path directory;

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
            explicit operator bool() const
            {
                return handle != nullptr && function != nullptr;
            }

        private:
            // Unload the library and remove its temporary directory
            void cleanup() noexcept;
        };

        // Wait for submitted source code and compile it
        void compiler_loop();

        // Compile source code and return the loaded plugin
        Plugin compile_source(const std::string& source);

        // Add a message to the engine log
        void write_log(const std::string& message);

        // Background compiler thread
        std::atomic<bool> running = false;
        std::thread compiler_thread;

        // Source code waiting to be compiled
        std::mutex source_mutex;
        std::condition_variable source_condition_variable;
        std::string pending_source;
        bool has_pending_source = false;

        // Newly compiled plugin waiting to be installed
        std::mutex plugin_mutex;
        Plugin pending_plugin;

        // Plugin currently being used by the audio thread
        Plugin active_plugin;

        // Used to safely swap plugins between the UI and audio threads
        std::atomic<bool> swap_requested = false;
        std::atomic<bool> swap_acknowledged = false;

        // Engine log
        mutable std::mutex log_mutex;
        std::vector<std::string> log;

        // Compiler configuration
        Settings settings;
    };
}
