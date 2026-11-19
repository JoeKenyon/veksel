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
        int channels;   // in the direction it's listed under
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
        int inputChannel = 0;       // 0 is the first input
        int outputDevice = -1;
        int outputChannel = 0;      // first of the output pair: 0 is 1-2, 2 is 3-4
        int sampleRate = 44100;
        int bufferSize = 64;

        bool operator==(const AudioSettings&) const = default;
    };

    class Audio
    {
    public:
        explicit Audio(Log& log);
        ~Audio();

        // stops any running stream and starts one with these settings.
        // -1 devices mean the default. on failure audio is left off
        bool start(Engine& engine, const AudioSettings& settings = {});

        void stop();        // closes the stream, keeps PortAudio initialized
        void shutdown();    // stop + terminate PortAudio

        AudioDevices getDevices() const;
        AudioSettings getSettings() const;   // last settings that started successfully
        bool isRunning() const noexcept;

    private:
        bool openStream(const AudioSettings& settings);

        static int callback(const void* input, void* output, unsigned long frames,
                            const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags,
                            void* userData);

        Log& m_log;
        PaStream* m_stream = nullptr;
        bool m_initialized = false;
        AudioSettings m_settings;

        // what the callback reads. only changed while the stream is closed
        Engine* m_engine = nullptr;
        int m_inChannels = 0;    // channels per interleaved input frame, 0 = no input
        int m_inChannel = 0;     // the one we read
        int m_outChannels = 2;   // channels per interleaved output frame
        int m_outChannel = 0;    // first of the pair we write
    };
}