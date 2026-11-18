#include "Engine.h"

#include <utility>

using namespace veksel;

Engine::Engine(Log& log)
    : m_log{log}
    , m_compiler{log}
{
}

// process a single audio sample, called by the audio callback
float Engine::processSample(float input) noexcept
{
    // a swap has been requested: tell the ui thread we are no longer using
    // the old plugin, and pass audio through until it is replaced
    if (m_swapRequested.load(std::memory_order_acquire))
    {
        m_swapAcknowledged.store(true, std::memory_order_release);
        return input;
    }

    return m_active.function ? m_active.function(input) : input;
}

void Engine::update()
{
    // collect a freshly compiled plugin. a newer one replaces one still
    // waiting, which is fine because a pending plugin was never active.
    if (auto next = m_compiler.take())
        m_pending = std::move(*next);

    if (!m_pending)
        return;

    // no handover in flight: ask the audio thread to stop using the active
    // plugin, then check for the acknowledgement on a later frame
    if (!m_swapRequested.load(std::memory_order_acquire))
    {
        m_log.write("[sys] New plugin ready. Waiting for audio thread.");

        m_swapAcknowledged.store(false, std::memory_order_relaxed);
        m_swapRequested.store(true, std::memory_order_release);
        return;
    }

    // the audio thread hasn't reached a safe point yet
    if (!m_swapAcknowledged.load(std::memory_order_acquire))
        return;

    m_log.write("[sys] Audio thread acknowledged plugin swap.");

    // install the new plugin. assignment destroys the old one and its jit
    // memory, which is safe now that the audio thread acknowledged.
    // exchange also leaves m_pending cleanly empty.
    m_active = std::exchange(m_pending, Plugin{});

    m_swapRequested.store(false, std::memory_order_release);

    m_log.write("[ok] Plugin successfully loaded.");
}

void Engine::submitSource(std::string source)
{
    m_compiler.submit(std::move(source));
}