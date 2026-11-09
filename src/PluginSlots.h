#pragma once

#include <juce_core/juce_core.h>

#include <atomic>
#include <memory>

using PluginFunc = float (*)(float);

struct Plugin
{
    std::unique_ptr<juce::DynamicLibrary> lib; // closes the library on destruction
    PluginFunc function = nullptr;
};

// hot-swap slots shared between the compiler thread and the audio thread
//   compiler -> pending -> (audio thread) current -> to_be_deleted -> compiler frees it
// the compiler thread must be stopped before this is destroyed
struct PluginSlots
{
    Plugin*              current = nullptr; // audio thread only
    std::atomic<Plugin*> pending{nullptr};
    std::atomic<Plugin*> to_be_deleted{nullptr};

    ~PluginSlots()
    {
        delete current;
        delete pending.exchange(nullptr);
        delete to_be_deleted.exchange(nullptr);
    }
};
