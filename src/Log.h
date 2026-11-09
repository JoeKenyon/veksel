#pragma once

#include <juce_core/juce_core.h>
#include <optional>

// thread-safe, capped at 100 lines
void push_log(const juce::String& msg);

// full log text, only if something was added since the last call
std::optional<juce::String> take_log_text();
