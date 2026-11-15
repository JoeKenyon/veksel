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

        bool start(Engine& engine, int inputDev = -1, int outputDev = -1, int sampleRate = 44100, int bufferSize = 64);
        void shutdown();
        AudioDevices getDevices() const;

    private:
        PaStream* m_stream = nullptr;
        bool m_initialized = false;
    };
}
