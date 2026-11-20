#include "Log.h"

#include <utility>

using namespace veksel;

void Log::write(std::string message)
{
    std::lock_guard lock(m_mutex);

    m_lines.push_back(std::move(message));

    // keep the newest lines only
    if (m_lines.size() > MaxLines)
        m_lines.pop_front();
}

std::deque<std::string> Log::getLines() const
{
    std::lock_guard lock(m_mutex);
    return m_lines;
}

void Log::clear()
{
    std::lock_guard lock(m_mutex);
    m_lines.clear();
}