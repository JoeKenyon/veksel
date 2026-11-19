#include "Engine.h"

#include <utility>

using namespace veksel;

Engine::Engine(Log& log)
    : m_log{log}
    , m_compiler{log}
{
}

Engine::~Engine()
{
    // audio stream is stopped by now, nobody else touches these
    delete m_active;
    delete m_incoming.load();
    delete m_retired.load();
}

void Engine::beginBlock() noexcept
{
    // new plugin waiting and the bin is empty: swap. never blocks
    if (m_incoming.load(std::memory_order_relaxed) &&
        !m_retired.load(std::memory_order_relaxed))
    {
        if (Plugin* next = m_incoming.exchange(nullptr, std::memory_order_acq_rel))
        {
            m_retired.store(m_active, std::memory_order_release);
            m_active = next;
        }
    }
}

float Engine::processSample(float input) noexcept
{
    // the compiler only hands out plugins with a valid function
    return m_active ? m_active->function(input) : input;
}

void Engine::update()
{
    // empty the bin. the audio thread is done with these
    delete m_retired.exchange(nullptr, std::memory_order_acq_rel);

    if (auto next = m_compiler.take())
    {
        // if the audio thread hasn't taken the last one yet, we get it back
        // and delete it. if it already took it, we get nullptr
        delete m_incoming.exchange(next.release(), std::memory_order_acq_rel);
        m_log.write("[ok] Plugin ready.");
    }
}

void Engine::submitSource(std::string source)
{
    m_compiler.submit(std::move(source));
}