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
        explicit operator bool() const noexcept { return state != nullptr; }
    };

    // compiles c source into plugins on a background thread
    class Compiler
    {   
    public:
        // need the log to log stuff
        // constructor will start the compiler thread
        explicit Compiler(Log& log);

        // stops and joins the thread
        ~Compiler();

        Compiler(const Compiler&) = delete;
        Compiler& operator=(const Compiler&) = delete;

        // queues source for compiling
        // replaces source that hasn't started yet
        void submit(std::string source);

        // the newest successfully compiled plugin, if there is one
        // each plugin is handed out once, a failed compile produces nothing.
        std::optional<Plugin> take();

    private:
        // compile some source code and return a plugin
        std::optional<Plugin> compile(std::string_view source);

        void loop();
        void logErrors(std::string_view text);
        Log& m_log;

        // guards everything below
        std::mutex m_mutex;                   
        std::condition_variable m_wake;

        // source code waiting to be compiled
        std::optional<std::string> m_source;

        // compiled, waiting to be taken
        std::optional<Plugin> m_result;       
        bool m_stopping = false;

        // our thread 
        std::thread m_thread;                 
    };
}