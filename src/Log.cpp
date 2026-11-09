#include "Log.h"

#include <atomic>

namespace
{
    juce::CriticalSection log_lock;
    juce::StringArray     log_lines;
    std::atomic<bool>     log_dirty{false};
}

void push_log(const juce::String& msg)
{
    const juce::ScopedLock sl(log_lock);
    log_lines.add(msg);
    if (log_lines.size() > 100)
        log_lines.remove(0);
    log_dirty = true;
}

std::optional<juce::String> take_log_text()
{
    if (! log_dirty.exchange(false))
        return std::nullopt;

    const juce::ScopedLock sl(log_lock);
    return log_lines.joinIntoString("\n");
}
