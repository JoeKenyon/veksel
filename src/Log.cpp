
#include "Log.h"

#include <utility>

using namespace veksel;

void Log::write(std::string message)
{
    Callback callback;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // save the message and copy the callback
        m_lines.push_back(message);
        callback = m_callback;
    }

    // notify the ui without holding the log mutex
    if (callback)
        callback(message);
}

std::vector<std::string> Log::getLines() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // return a copy so the internal vector stays protected
    return m_lines;
}

void Log::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // remove all messages
    m_lines.clear();
}

void Log::setCallback(Callback callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // replace the current callback
    m_callback = std::move(callback);
}
