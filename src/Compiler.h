#pragma once

#include "Log.h"

#include <atomic>
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

        // queues source, replacing anything waiting to compile
        void submit(std::string source);

        std::atomic<Plugin*>& incoming() noexcept { return m_incoming; }
        std::atomic<Plugin*>& retired()  noexcept { return m_retired;  }

    private:
        std::unique_ptr<Plugin> compile(std::string_view source);
        void loop();
        void logErrors(std::string_view text);

        Log& m_log;

        // lock-free handoff slots for the audio thread
        std::atomic<Plugin*> m_incoming{nullptr};
        std::atomic<Plugin*> m_retired{nullptr};

        // guards compilation requests and shutdown state
        std::mutex m_mutex;
        std::condition_variable m_wake;
        std::optional<std::string> m_source;
        bool m_stopping = false;

        // last so members above are ready before the thread starts
        std::thread m_thread;
    };
}