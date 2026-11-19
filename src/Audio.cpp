#include "Audio.h"

#include <algorithm>
#include <cmath>

namespace veksel
{
    namespace
    {
        constexpr double SuggestedLatency = 0.01;   // seconds

        PaStreamParameters makeParams(int device, int channels)
        {
            PaStreamParameters p = {};
            p.device = device;
            p.channelCount = channels;
            p.sampleFormat = paFloat32;
            p.suggestedLatency = SuggestedLatency;
            return p;
        }
    }

    Audio::Audio(Log& log) : m_log(log) {}

    Audio::~Audio() { shutdown(); }

    // audio thread: no logging, locking or allocating in here
    int Audio::callback(const void* input, void* output, unsigned long frames,
                        const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags,
                        void* userData)
    {


        auto& a = *static_cast<Audio*>(userData);
        const auto* in = static_cast<const float*>(input);
        auto* out = static_cast<float*>(output);

        if (!a.m_engine || !out)
            return paContinue;

        // one swap check per block, then the plugin can't change under us
        a.m_engine->beginBlock();

        for (unsigned long i = 0; i < frames; ++i)
        {
            // interleaved, so pick our channel out of the frame. no input = silence
            float s = in ? in[i * a.m_inChannels + a.m_inChannel] : 0.0f;
            s = a.m_engine->processSample(s);

            // plugins can make nan, inf or huge values. protect the speakers
            if (!std::isfinite(s))
                s = 0.0f;
            s = std::clamp(s, -1.0f, 1.0f);

            // silence every channel except our pair. an odd leftover has no second one
            float* frame = out + i * a.m_outChannels;
            std::fill(frame, frame + a.m_outChannels, 0.0f);

            frame[a.m_outChannel] = s;
            if (a.m_outChannel + 1 < a.m_outChannels)
                frame[a.m_outChannel + 1] = s;
        }

        return paContinue;
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

        stop();

        // -1 means the default device
        AudioSettings s = settings;
        if (s.inputDevice < 0)  s.inputDevice  = Pa_GetDefaultInputDevice();
        if (s.outputDevice < 0) s.outputDevice = Pa_GetDefaultOutputDevice();

        m_engine = &engine;

        if (!openStream(s))
            return false;

        m_settings = s;
        m_log.write("[sys] Audio started (" + std::to_string(s.sampleRate) + " Hz, " +
                    std::to_string(s.bufferSize) + " frames).");
        return true;
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

    bool Audio::openStream(const AudioSettings& s)
    {
        const PaDeviceInfo* inInfo  = Pa_GetDeviceInfo(s.inputDevice);
        const PaDeviceInfo* outInfo = Pa_GetDeviceInfo(s.outputDevice);

        // a device with no input channels is the same as no input device
        if (inInfo && inInfo->maxInputChannels < 1)
            inInfo = nullptr;

        if (!outInfo || outInfo->maxOutputChannels < 1)
        {
            m_log.write("[error] No output device available.");
            return false;
        }

        // portaudio opens channels 0..n, so to read input 3 we open 4 and
        // ignore the first 3. output is the same, up to the end of our pair
        m_inChannel   = inInfo ? std::clamp(s.inputChannel, 0, inInfo->maxInputChannels - 1) : 0;
        m_inChannels  = inInfo ? m_inChannel + 1 : 0;
        m_outChannel  = std::clamp(s.outputChannel, 0, outInfo->maxOutputChannels - 1);
        m_outChannels = std::min(m_outChannel + 2, outInfo->maxOutputChannels);

        const PaStreamParameters inParams  = makeParams(s.inputDevice, m_inChannels);
        const PaStreamParameters outParams = makeParams(s.outputDevice, m_outChannels);

        if (Pa_OpenStream(&m_stream, inInfo ? &inParams : nullptr, &outParams,
                          s.sampleRate, s.bufferSize, paClipOff, callback, this) != paNoError)
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
        AudioDevices d;

        for (int i = 0; i < Pa_GetDeviceCount(); ++i)
        {
            const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
            if (!info)
                continue;

            if (info->maxInputChannels > 0)
                d.inputs.push_back({i, info->name, info->maxInputChannels});
            if (info->maxOutputChannels > 0)
                d.outputs.push_back({i, info->name, info->maxOutputChannels});
        }

        // fixed lists, we don't ask what each device supports
        d.sampleRates = {44100, 48000, 88200, 96000};
        d.bufferSizes = {64, 128, 256, 512, 1024};
        return d;
    }

    AudioSettings Audio::getSettings() const { return m_settings; }

    bool Audio::isRunning() const noexcept { return m_stream != nullptr; }
}