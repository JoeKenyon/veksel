#include "Audio.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace veksel
{
    namespace
    {
        // we take one mono input and send the same signal to both output channels
        // change this later 
        constexpr int InputChannels = 1;
        constexpr int OutputChannels = 2;

        // seconds, how much latency we ask portaudio for
        constexpr double SuggestedLatency = 0.01;

        PaStreamParameters makeParams(int device, int channels)
        {
            PaStreamParameters params = {};
            params.device = device;
            params.channelCount = channels;
            params.sampleFormat = paFloat32;
            params.suggestedLatency = SuggestedLatency;
            return params;
        }

        // runs on the audio thread, so no logging, locking or allocating in here
        int audioCallback( const void* input, void* output, unsigned long frames, const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void* userData)
        {
            auto* engine = static_cast<Engine*>(userData);
            const auto* in = static_cast<const float*>(input);
            auto* out = static_cast<float*>(output);

            for (unsigned long i = 0; i < frames; ++i)
            {
                // no input device means we feed silence
                float sample = engine->processSample(in ? in[i] : 0.0f);

                // plugins can produce nan, inf or huge values, keep the speakers safe
                if (!std::isfinite(sample))
                    sample = 0.0f;

                sample = std::clamp(sample, -1.0f, 1.0f);

                out[i * 2]     = sample;
                out[i * 2 + 1] = sample;
            }

            return paContinue;
        }
    }

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

        // -1 means "use the default device"
        const AudioSettings requested{
            inputDev  < 0 ? Pa_GetDefaultInputDevice()  : inputDev,
            outputDev < 0 ? Pa_GetDefaultOutputDevice() : outputDev,
            sampleRate,
            bufferSize
        };

        // remember what was running so we can go back to it if this fails
        const AudioSettings previous = m_settings;
        const bool hadStream = isRunning();

        stop();

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

        // the new settings failed, so put the old stream back and keep audio going
        if (hadStream)
        {
            if (openStream(engine, previous))
            {
                m_settings = previous;
                m_log.write("[warn] Previous audio settings restored.");
            }
            else
            {
                m_log.write("[error] Failed to restore previous audio settings.");
            }
        }

        return false;
    }

    void Audio::stop()
    {
        if (!m_stream)
            return;

        Pa_StopStream(m_stream);
        Pa_CloseStream(m_stream);
        m_stream = nullptr;
    }

    void Audio::shutdown()
    {
        stop();

        if (m_initialized)
        {
            Pa_Terminate();
            m_initialized = false;
        }
    }

    bool Audio::openStream(Engine& engine, const AudioSettings& settings)
    {
        const PaStreamParameters inParams  = makeParams(settings.inputDevice,  InputChannels);
        const PaStreamParameters outParams = makeParams(settings.outputDevice, OutputChannels);

        // no input device is fine, we just run output only
        const PaStreamParameters* inPtr = inParams.device != paNoDevice ? &inParams : nullptr;

        const PaError openResult = Pa_OpenStream(
            &m_stream,
            inPtr,
            &outParams,
            settings.sampleRate,
            settings.bufferSize,
            paClipOff,          // we clamp ourselves in the callback
            audioCallback,
            &engine
        );

        if (openResult != paNoError)
        {
            m_stream = nullptr;
            m_log.write("[error] Failed to open audio stream.");
            return false;
        }

        if (Pa_StartStream(m_stream) != paNoError)
        {
            Pa_CloseStream(m_stream);
            m_stream = nullptr;
            m_log.write("[error] Failed to start audio stream.");
            return false;
        }

        return true;
    }

    AudioDevices Audio::getDevices() const
    {
        AudioDevices devices;

        for (int i = 0; i < Pa_GetDeviceCount(); ++i)
        {
            const PaDeviceInfo* info = Pa_GetDeviceInfo(i);

            if (!info)
                continue;

            if (info->maxInputChannels > 0)
                devices.inputs.push_back({i, info->name});

            if (info->maxOutputChannels > 0)
                devices.outputs.push_back({i, info->name});
        }

        // fixed lists, we don't query what each device actually supports
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