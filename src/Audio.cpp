#include "Audio.h"
#include <algorithm>
#include <cmath>

namespace veksel
{
    Audio::Audio(Log& log)
        : m_log(log)
    {
    }

    Audio::~Audio()
    {
        shutdown();
    }

    bool Audio::start(Engine& engine, int inputDev, int outputDev, int sampleRate, int bufferSize)
    {
        if (!m_initialized)
        {
            if (Pa_Initialize() != paNoError)
            {
                m_log.write("[error] Failed to initialize PortAudio.");
                return false;
            }

            m_initialized = true;
        }

        const AudioSettings requested{
            inputDev,
            outputDev,
            sampleRate,
            bufferSize
        };

        const AudioSettings previous = m_settings;
        const bool hadStream = m_stream != nullptr;

        if (m_stream)
        {
            Pa_StopStream(m_stream);
            Pa_CloseStream(m_stream);
            m_stream = nullptr;
        }

        if (openStream(engine, requested))
        {
            m_settings = requested;

            m_log.write(
                "[sys] Audio stream started successfully (" +
                std::to_string(sampleRate) + " Hz, " +
                std::to_string(bufferSize) + " frames)."
            );

            return true;
        }

        // restore the previous configuration if the new one fails
        if (hadStream && openStream(engine, previous))
        {
            m_settings = previous;
            m_log.write("[warn] Previous audio settings restored.");
        }
        else if (hadStream)
        {
            m_log.write("[error] Failed to restore previous audio settings.");
        }

        return false;
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

    bool Audio::openStream(Engine& engine, const AudioSettings& settings)
    {
        PaStreamParameters inParams = {};
        inParams.device = settings.inputDevice < 0
            ? Pa_GetDefaultInputDevice()
            : settings.inputDevice;
        inParams.channelCount = 1;
        inParams.sampleFormat = paFloat32;
        inParams.suggestedLatency = 0.01;

        PaStreamParameters outParams = {};
        outParams.device = settings.outputDevice < 0
            ? Pa_GetDefaultOutputDevice()
            : settings.outputDevice;
        outParams.channelCount = 2;
        outParams.sampleFormat = paFloat32;
        outParams.suggestedLatency = 0.01;

        const PaStreamParameters* inPtr =
            inParams.device != paNoDevice ? &inParams : nullptr;

        auto callback = [](
            const void* input,
            void* output,
            unsigned long frames,
            const PaStreamCallbackTimeInfo*,
            PaStreamCallbackFlags,
            void* userData
        ) -> int
        {
            auto* eng = static_cast<Engine*>(userData);
            const float* in = static_cast<const float*>(input);
            float* out = static_cast<float*>(output);

            for (unsigned long i = 0; i < frames; ++i)
            {
                float sample = in ? in[i] : 0.0f;
                sample = eng->processSample(sample);

                if (!std::isfinite(sample))
                    sample = 0.0f;

                sample = std::clamp(sample, -1.0f, 1.0f);

                out[i * 2] = sample;
                out[i * 2 + 1] = sample;
            }

            return paContinue;
        };

        if (Pa_OpenStream(
            &m_stream,
            inPtr,
            &outParams,
            settings.sampleRate,
            settings.bufferSize,
            paClipOff,
            callback,
            &engine
        ) != paNoError)
        {
            m_stream = nullptr;
            m_log.write("[error] Failed to open audio stream.");
            return false;
        }

        if (Pa_StartStream(m_stream) != paNoError)
        {
            m_log.write("[error] Failed to start audio stream.");
            Pa_CloseStream(m_stream);
            m_stream = nullptr;
            return false;
        }

        return true;
    }

    AudioDevices Audio::getDevices() const
    {
        AudioDevices devices;

        const int count = Pa_GetDeviceCount();

        for (int i = 0; i < count; ++i)
        {
            const PaDeviceInfo* info = Pa_GetDeviceInfo(i);

            if (!info)
                continue;

            if (info->maxInputChannels > 0)
                devices.inputs.push_back({i, info->name});

            if (info->maxOutputChannels > 0)
                devices.outputs.push_back({i, info->name});
        }

        devices.sampleRates = {44100, 48000, 88200, 96000};
        devices.bufferSizes = {64, 128, 256, 512, 1024};

        return devices;
    }

    AudioSettings Audio::getSettings() const
    {
        return m_settings;
    }

    bool Audio::isRunning() const noexcept
    {
        return m_stream != nullptr;
    }
}