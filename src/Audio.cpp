#include "Audio.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace veksel
{
    namespace
    {
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
        int audioCallback(
            const void* input,
            void* output,
            unsigned long frames,
            const PaStreamCallbackTimeInfo*,
            PaStreamCallbackFlags,
            void* userData)
        {
            const auto* ctx = static_cast<const StreamContext*>(userData);
            const auto* in = static_cast<const float*>(input);
            auto* out = static_cast<float*>(output);

            for (unsigned long i = 0; i < frames; ++i)
            {
                // samples are interleaved, so pick our channel out of the frame.
                // no input device means we feed silence
                float sample = in ? in[i * ctx->inChannels + ctx->inChannel] : 0.0f;
                sample = ctx->engine->processSample(sample);

                // plugins can produce nan, inf or huge values, keep the speakers safe
                if (!std::isfinite(sample))
                    sample = 0.0f;

                sample = std::clamp(sample, -1.0f, 1.0f);

                // silence every channel except our pair. the second one is
                // missing if the device has an odd channel left over
                float* frame = out + i * ctx->outChannels;
                std::fill(frame, frame + ctx->outChannels, 0.0f);

                frame[ctx->outChannel] = sample;
                if (ctx->outChannel + 1 < ctx->outChannels)
                    frame[ctx->outChannel + 1] = sample;
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

    bool Audio::start(Engine& engine, const AudioSettings& settings)
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
        AudioSettings requested = settings;

        if (requested.inputDevice < 0)
            requested.inputDevice = Pa_GetDefaultInputDevice();

        if (requested.outputDevice < 0)
            requested.outputDevice = Pa_GetDefaultOutputDevice();

        // remember what was running so we can go back to it if this fails
        const AudioSettings previous = m_settings;
        const bool hadStream = isRunning();

        stop();

        if (openStream(engine, requested))
        {
            m_settings = requested;

            m_log.write(
                "[sys] Audio stream started successfully (" +
                std::to_string(requested.sampleRate) + " Hz, " +
                std::to_string(requested.bufferSize) + " frames)."
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
        const PaDeviceInfo* inInfo  = Pa_GetDeviceInfo(settings.inputDevice);
        const PaDeviceInfo* outInfo = Pa_GetDeviceInfo(settings.outputDevice);

        if (!outInfo)
        {
            m_log.write("[error] No output device available.");
            return false;
        }

        // portaudio opens channels 0..n, so to read input channel 3 we open
        // 4 and ignore the first 3. same for output, we open up to the end
        // of our pair and zero the rest in the callback
        m_context = StreamContext{};
        m_context.engine = &engine;

        if (inInfo)
        {
            m_context.inChannel  = std::clamp(settings.inputChannel, 0, inInfo->maxInputChannels - 1);
            m_context.inChannels = m_context.inChannel + 1;
        }

        m_context.outChannel  = std::clamp(settings.outputChannel, 0, outInfo->maxOutputChannels - 1);
        m_context.outChannels = std::min(m_context.outChannel + 2, outInfo->maxOutputChannels);

        const PaStreamParameters inParams  = makeParams(settings.inputDevice, m_context.inChannels);
        const PaStreamParameters outParams = makeParams(settings.outputDevice, m_context.outChannels);

        // no input device is fine, we just run output only
        const PaStreamParameters* inPtr = inInfo ? &inParams : nullptr;

        const PaError openResult = Pa_OpenStream(
            &m_stream,
            inPtr,
            &outParams,
            settings.sampleRate,
            settings.bufferSize,
            paClipOff,          // we clamp ourselves in the callback
            audioCallback,
            &m_context
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
                devices.inputs.push_back({i, info->name, info->maxInputChannels});

            if (info->maxOutputChannels > 0)
                devices.outputs.push_back({i, info->name, info->maxOutputChannels});
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