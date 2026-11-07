#pragma once

#include "Plugin.h"
#include "portaudio.h" // correct paths?

// Owns the PortAudio stream and the plugin handoff.
//
// Slots:
//   active_  — audio thread only
//   pending_ — compiler publishes, audio consumes
//   retired_ — audio publishes, compiler destroys
//
// Audio swaps at the start of a callback. The compiler does all dlclose
// off the audio thread. Every plugin is freed exactly once.
class AudioEngine
{
public:
    AudioEngine() = default;
    ~AudioEngine();

    // Open the stream at the device's default rate. Returns false on failure.
    bool start();

    // Stop and close the stream. Blocks until the callback has returned.
    // Returns true if the stream is known to be stopped.
    bool stop();

    // Publish a new plugin from the compiler thread. Takes ownership.
    // If an older build is still pending, it's destroyed (newest wins! ya hoo).
    void publish(PluginPtr p);

    // Free whatever the audio thread handed back as retired.
    // Call from the compiler thread only.
    void reclaim();

private:

    // portaudio callback sig
    static int callback(
        const void*,
        void*,
        unsigned long,
        const PaStreamCallbackTimeInfo*,
        PaStreamCallbackFlags,
        void*);

    void swap();

    PaStream* stream_ = nullptr;
    PluginPtr active_;
    std::atomic<Plugin*>  pending_{nullptr};
    std::atomic<Plugin*>  retired_{nullptr};
};
