#pragma once

#include "Compiler.h"
#include "Log.h"

#include <atomic>
#include <string>

namespace veksel
{
    // runs the active plugin on the audio thread and swaps in new ones
    // safely as the compiler produces them
    class Engine
    {
    public:
        // the compiler thread starts here and is joined on destruction.
        // the audio stream must be stopped before the engine is destroyed,
        // so processSample() can never run against a destroyed engine.
        explicit Engine(Log& log);

        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;

        // must only be called from one serial audio callback thread.
        // the swap acknowledgement relies on sequential invocation.
        float processSample(float input) noexcept;

        // called from the ui thread once per frame.
        // installs a freshly compiled plugin once the audio thread has acknowledged.
        void update();

        // queue source for compiling on the compiler thread
        void submitSource(std::string source);

    private:
        Log& m_log;

        // read by the audio thread, written by the ui thread
        // only after m_swapAcknowledged is observed
        Plugin m_active;

        // compiled, waiting for the audio thread to let go of the active one.
        // only touched by the ui thread, so it needs no lock
        Plugin m_pending;

        // two-flag swap handshake
        std::atomic<bool> m_swapRequested{false};     // ui -> audio
        std::atomic<bool> m_swapAcknowledged{false};  // audio -> ui

        // last, since its thread starts in the constructor
        Compiler m_compiler;
    };
}