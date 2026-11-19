#pragma once

#include "Log.h"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

struct TCCState;

namespace veksel
{
    using ProcessFunc = float (*)(float);

    struct TccDeleter
    {
        void operator()(TCCState* state) const noexcept;
    };

    struct Plugin
    {
        ProcessFunc function = nullptr;
        std::unique_ptr<TCCState, TccDeleter> state;
    };

    // compiles c source into plugins on a background thread
    class Compiler
    {
    public:
        explicit Compiler(Log& log);
        ~Compiler();

        Compiler(const Compiler&) = delete;
        Compiler& operator=(const Compiler&) = delete;

        // queues source. replaces source that hasn't started yet
        void submit(std::string source);

        // newest compiled plugin, or null. each one is handed out once
        std::unique_ptr<Plugin> take();

    private:
        std::unique_ptr<Plugin> compile(std::string_view source);
        void loop();
        void logErrors(std::string_view text);

        Log& m_log;

        // guards everything below
        std::mutex m_mutex;
        std::condition_variable m_wake;
        std::optional<std::string> m_source;
        std::unique_ptr<Plugin> m_result;
        bool m_stopping = false;

        // last, its thread starts in the constructor
        std::thread m_thread;
    };
}