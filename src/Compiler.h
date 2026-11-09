#pragma once

#include "PluginSlots.h"

#include <juce_core/juce_core.h>
#include <optional>

// compiles submitted source with clang and hands the result to the pending slot
class CompilerThread : public juce::Thread
{
public:
    explicit CompilerThread(PluginSlots& s);

    void set_code(const juce::String& s);
    void run() override;

private:
    PluginSlots&                slots;
    juce::CriticalSection       code_lock;
    std::optional<juce::String> code;
};
