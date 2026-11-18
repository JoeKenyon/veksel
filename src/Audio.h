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
        int outputDevice = -1;
        int sampleRate = 44100;
        int bufferSize = 64;
    };

    class Audio
    {
    public:
        explicit Audio(Log& log);
        ~Audio();

        bool start(Engine& engine, int inputDev = -1, int outputDev = -1, int sampleRate = 44100, int bufferSize = 64);

        // stops and closes the stream.
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
    };
}