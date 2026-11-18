#pragma once

#include "Engine.h"
#include "Log.h"
#include <portaudio.h>
#include <string>
#include <vector>

namespace veksel
{
    struct AudioDevice
    {
        int index;
        std::string name;

        // channel count in the direction this device is listed under
        int channels;
    };

    struct AudioDevices
    {
        std::vector<AudioDevice> inputs;
        std::vector<AudioDevice> outputs;

        std::vector<int> sampleRates;
        std::vector<int> bufferSizes;
    };

    struct AudioSettings
    {
        int inputDevice = -1;
        int inputChannel = 0;       // which input on the device, 0 is the first
        int outputDevice = -1;
        int outputChannel = 0;      // first channel of the output pair, 0 is 1-2, 2 is 3-4
        int sampleRate = 44100;
        int bufferSize = 64;

        bool operator==(const AudioSettings&) const = default;
    };

    // what the audio callback needs to know. internal to Audio, it just has to
    // be visible so the callback can name it
    struct StreamContext
    {
        Engine* engine = nullptr;
        int inChannels = 1;     // channels in each interleaved input frame
        int inChannel = 0;      // the one we read
        int outChannels = 2;    // channels in each interleaved output frame
        int outChannel = 0;     // first of the pair we write
    };

    class Audio
    {
    public:
        explicit Audio(Log& log);
        ~Audio();

        // stops any running stream and starts one with these settings.
        // -1 devices mean the default. if it fails the old stream comes back
        bool start(Engine& engine, const AudioSettings& settings = {});

        // stops and closes the stream (keeps PortAudio initialized).
        void stop();

        // stops the stream and terminates PortAudio.
        void shutdown();

        // enumerates portaudio devices.
        AudioDevices getDevices() const;
        AudioSettings getSettings() const;
        bool isRunning() const noexcept;

    private:
        bool openStream(Engine& engine, const AudioSettings& settings);

        Log& m_log;
        PaStream* m_stream = nullptr;
        bool m_initialized = false;
        AudioSettings m_settings;

        // only changed while the stream is closed, so the callback never sees it move
        StreamContext m_context;
    };
}