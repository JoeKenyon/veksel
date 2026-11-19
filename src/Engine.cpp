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
    // m_compiler has already joined its thread, and the audio stream is stopped
    delete m_active;
    delete m_compiler.incoming().exchange(nullptr, std::memory_order_acq_rel);
    delete m_compiler.retired().exchange(nullptr, std::memory_order_acq_rel);
}

void Engine::beginBlock() noexcept
{
    // only swap when the retired bin is clear and a new plugin is waiting
    if (!m_compiler.retired().load(std::memory_order_acquire) &&
        m_compiler.incoming().load(std::memory_order_acquire))
    {
        if (Plugin* next = m_compiler.incoming().exchange(nullptr, std::memory_order_acq_rel))
        {
            if (m_active)
            {
                m_compiler.retired().store(m_active, std::memory_order_release);
            }
            m_active = next;
        }
    }
}

float Engine::processSample(float input) noexcept
{
    // the compiler only hands out plugins with a valid function
    return m_active ? m_active->function(input) : input;
}

void Engine::submitSource(std::string source)
{
    m_compiler.submit(std::move(source));
}