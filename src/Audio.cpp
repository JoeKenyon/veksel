#include "Audio.h"
#include <algorithm>
#include <cmath>

namespace veksel
{
    Audio::Audio() = default;

    Audio::~Audio()
    {
        shutdown();
    }

    bool Audio::start(Engine& engine, int inputDev, int outputDev, int sampleRate, int bufferSize)
    {
        if (!m_initialized)
        {
            if (Pa_Initialize() != paNoError)
                return false;

            m_initialized = true;
        }
        else if (m_stream)
        {
            Pa_StopStream(m_stream);
            Pa_CloseStream(m_stream);
            m_stream = nullptr;
        }

        PaStreamParameters inParams = {};
        inParams.device = (inputDev < 0) ? Pa_GetDefaultInputDevice() : inputDev;
        inParams.channelCount = 1;   // mono in: portaudio downmixes, matches the mono plugin ABI
        inParams.sampleFormat = paFloat32;
        inParams.suggestedLatency = 0.01;

        PaStreamParameters outParams = {};
        outParams.device = (outputDev < 0) ? Pa_GetDefaultOutputDevice() : outputDev;
        outParams.channelCount = 2;   // stereo out: the mono result is duplicated to both channels
        outParams.sampleFormat = paFloat32;
        outParams.suggestedLatency = 0.01;

        const PaStreamParameters* inPtr = (inParams.device != paNoDevice) ? &inParams : nullptr;

        auto callback = [](const void* input, void* output, unsigned long frames,
                           const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void* userData) -> int
        {
            auto* eng = static_cast<Engine*>(userData);
            const float* in = static_cast<const float*>(input);
            float* out = static_cast<float*>(output);

            for (unsigned long i = 0; i < frames; ++i)
            {
                float sample = in ? in[i] : 0.0f;   // mono in
                sample = eng->processSample(sample);

                if (!std::isfinite(sample))
                    sample = 0.0f;

                sample = std::clamp(sample, -1.0f, 1.0f);

                // duplicate the mono result to both output channels
                out[i * 2]     = sample;
                out[i * 2 + 1] = sample;
            }

            return paContinue;
        };

        if (Pa_OpenStream(&m_stream, inPtr, &outParams, sampleRate, bufferSize, paClipOff, callback, &engine) != paNoError)
        {
            engine.writeLog("[error] Failed to open audio stream.");
            return false;
        }

        if (Pa_StartStream(m_stream) != paNoError)
        {
            engine.writeLog("[error] Failed to start audio stream.");
            Pa_CloseStream(m_stream);
            m_stream = nullptr;
            return false;
        }

        return true;
    }

    void Audio::shutdown()
    {
        if (m_stream)
        {
            Pa_StopStream(m_stream);
            Pa_CloseStream(m_stream);
            m_stream = nullptr;
        }

        if (m_initialized)
        {
            Pa_Terminate();
            m_initialized = false;
        }
    }

    AudioDevices Audio::getDevices() const
    {
        AudioDevices devices;

        // populate portaudio input/output devices
        int count = Pa_GetDeviceCount();
        for (int i = 0; i < count; ++i)
        {
            const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
            if (!info)
                continue;

            if (info->maxInputChannels > 0)
            {
                devices.inputNames += info->name;
                devices.inputNames.push_back('\0');
                devices.inputIndices.push_back(i);
            }

            if (info->maxOutputChannels > 0)
            {
                devices.outputNames += info->name;
                devices.outputNames.push_back('\0');
                devices.outputIndices.push_back(i);
            }
        }

        devices.inputNames.push_back('\0');
        devices.outputNames.push_back('\0');

        // fallbacks must store both nul bytes, otherwise the multi-string walk
        // in custom_combo reads past the end.
        if (devices.inputIndices.empty())
            devices.inputNames.assign("No Inputs\0\0", 11);
        if (devices.outputIndices.empty())
            devices.outputNames.assign("No Outputs\0\0", 12);

        // populate sample rates and buffer sizes configuration options
        const std::vector<int> sampleRatesList = { 44100, 48000, 88200, 96000 };
        for (int sr : sampleRatesList)
        {
            devices.sampleRates += std::to_string(sr) + " Hz";
            devices.sampleRates.push_back('\0');
            devices.sampleIndices.push_back(sr);
        }
        devices.sampleRates.push_back('\0');

        const std::vector<int> bufferSizesList = { 64, 128, 256, 512, 1024 };
        for (int buf : bufferSizesList)
        {
            devices.bufferSizes += std::to_string(buf);
            devices.bufferSizes.push_back('\0');
            devices.bufferIndices.push_back(buf);
        }
        devices.bufferSizes.push_back('\0');

        return devices;
    }
}
