#pragma once

#include "Compiler.h"
#include "Log.h"

#include <string>

namespace veksel
{
    // runs the active plugin on the audio thread and swaps in new ones
    // without the audio thread ever waiting
    class Engine
    {
    public:
        // the compiler thread starts here and is joined on destruction.
        // the audio stream must be stopped before the engine is destroyed
        explicit Engine(Log& log);
        ~Engine();

        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;

        // audio thread only. call once at the start of every block.
        // picks up a new plugin if one is waiting
        void beginBlock() noexcept;

        // audio thread only. runs the active plugin, nothing else
        float processSample(float input) noexcept;

        // queue source for compiling on the compiler thread
        void submitSource(std::string source);

    private:
        Log& m_log;

        // audio thread only
        Plugin* m_active = nullptr;

        // last, its thread starts in the constructor
        Compiler m_compiler;
    };
}