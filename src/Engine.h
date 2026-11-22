#pragma once

#include "Log.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <string>
#include <thread>
/*
how this works:

1.  ui thread calls submitSource(), dropping the c code into m_pendingSource
    and waking the background compiler thread.

2.  compiler thread owns everything to do with libtcc. it builds
    a brand-new Plugin, then swaps it into m_pendingPlugin. 
    if an older build was still sitting there unconsumed
    (e.g. audio paused), it deletes that unconsumed one right away.

3.  audio thread calls beginBlock() once per block. if a new plugin is waiting
    in m_pendingPlugin, it grabs it and pushes the old m_activePlugin onto
    m_retiredQueue. audio never locks, allocates, or calls tcc_delete.

4.  compiler thread wakes every 100ms (and before every compile) to drain
    m_retiredQueue and delete old plugins safely off the audio thread.
*/

namespace veksel
{
    class Engine
    {
    public:
        // starts the compiler thread; destructor joins it.
        // stop the audio stream before destroying the engine
        explicit Engine(Log& log);
        ~Engine();

        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;

        // audio thread only. call once at the start of every block
        void beginBlock() noexcept;

        // audio thread only. runs the active plugin (or passes input through)
        float processSample(float input) noexcept;

        // ui thread: queue c source for background compilation
        void submitSource(std::string source);

        // ui hint so we can disable the compile button while tcc is busy
        bool isCompiling() const noexcept { return m_compiling.load(std::memory_order_relaxed); }

    private:
        struct Plugin; // defined in Engine.cpp so libtcc.h doesn't leak out

        // single-producer (audio) -> single-consumer (compiler) ring buffer for old plugins
        template <typename T, size_t Capacity>
        class SpscQueue
        {
            static_assert((Capacity & (Capacity - 1)) == 0, "capacity must be a power of 2");

        public:
            bool push(T item) noexcept
            {
                const size_t t    = m_tail.load(std::memory_order_relaxed);
                const size_t next = (t + 1) & (Capacity - 1);
                if (next == m_head.load(std::memory_order_acquire))
                    return false;
                m_buffer[t] = item;
                m_tail.store(next, std::memory_order_release);
                return true;
            }

            bool pop(T& item) noexcept
            {
                const size_t h = m_head.load(std::memory_order_relaxed);
                if (h == m_tail.load(std::memory_order_acquire))
                    return false;
                item = m_buffer[h];
                m_head.store((h + 1) & (Capacity - 1), std::memory_order_release);
                return true;
            }

        private:
            alignas(64) std::array<T, Capacity> m_buffer{};
            alignas(64) std::atomic<size_t> m_head{0}; // compiler thread only
            alignas(64) std::atomic<size_t> m_tail{0}; // audio thread only
        };

        void compilerThreadLoop();
        Plugin* compileWithTcc(const std::string& source);
        void drainRetired();

        Log& m_log;

        std::atomic<Plugin*>   m_pendingPlugin{nullptr};
        SpscQueue<Plugin*, 32> m_retiredQueue;
        Plugin*                m_activePlugin = nullptr; // audio thread only

        std::mutex              m_compilerMutex;
        std::condition_variable m_compilerCv;
        std::string             m_pendingSource;
        bool                    m_hasPendingSource = false;
        bool                    m_stopRequested    = false;
        std::atomic<bool>       m_compiling{false};

        // keep last so everything above is ready before the thread starts
        std::thread m_compilerThread;
    };
}