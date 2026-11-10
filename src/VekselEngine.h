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
        struct Settings
        {
            std::string compiler = "clang";
        };

        Engine();
        explicit Engine(Settings settings);
        ~Engine();

        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;

        // Compiler thread
        void start();
        void stop();

        // Submit source code for compilation
        void submit_source(const std::string& source);

        // Check for completed compilation and swap plugins
        // Call from the UI/control thread.
        void update();

        // Process one audio sample
        float process_sample(float input);

        // logging
        std::vector<std::string> get_log() const;
        void clear_log();

    private:
        using ProcessFunc = float (*)(float);

        struct Plugin
        {
            ProcessFunc function = nullptr;
            void* handle = nullptr;
            std::filesystem::path directory;

            ~Plugin();

            Plugin() = default;

            Plugin(const Plugin&) = delete;
            Plugin& operator=(const Plugin&) = delete;

            Plugin(Plugin&& other) noexcept;
            Plugin& operator=(Plugin&& other) noexcept;

            explicit operator bool() const
            {
                return handle != nullptr && function != nullptr;
            }
        private:
            void cleanup() noexcept;
        };

        void compiler_loop();
        Plugin compile_source(const std::string& source);

        void write_log(const std::string& message);

        // Compiler thread
        std::atomic<bool> running = false;
        std::thread compiler_thread;

        std::mutex source_mutex;
        std::condition_variable source_condition_variable;
        std::string pending_source;
        bool has_pending_source = false;

        // compiled plugin waiting for installation
        std::mutex plugin_mutex;
        Plugin pending_plugin;

        // Currently active plugin
        Plugin active_plugin;

        // audio/ui thread handshake
        std::atomic<bool> swap_requested = false;
        std::atomic<bool> swap_acknowledged = false;

        // logging stuff
        mutable std::mutex log_mutex;
        std::vector<std::string> log;

        Settings settings;
    };
}
