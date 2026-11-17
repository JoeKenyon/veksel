#pragma once

#include "VekselEngine.h"
#include <portaudio.h>
#include <string>
#include <vector>

namespace veksel
{
    struct AudioDevices
    {
        std::string inputNames;
        std::string outputNames;
        std::vector<int> inputIndices;
        std::vector<int> outputIndices;

        std::string sampleRates;
        std::string bufferSizes;
        std::vector<int> sampleIndices;
        std::vector<int> bufferIndices;
    };

    class Audio
    {
    public:
        Audio();
        ~Audio();

        // opens and starts a portaudio stream that calls engine.processSample().
        // the engine must outlive this Audio object (or shutdown() must be
        // called before the engine is destroyed), otherwise the audio callback
        // can run against a dead engine.
        bool start(Engine& engine, int inputDev = -1, int outputDev = -1, int sampleRate = 44100, int bufferSize = 64);

        // stops and closes the stream. call this before destroying the engine.
        void shutdown();

        // enumerates portaudio devices. must not race with start()/shutdown().
        AudioDevices getDevices() const;

    private:
        PaStream* m_stream = nullptr;
        bool m_initialized = false;
    };
}
