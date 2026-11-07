#include "AudioEngine.h"
#include "HostState.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

// ---------------------------------------------------------------------------
// Audio callback + swap
// ---------------------------------------------------------------------------
void AudioEngine::swap()
{
    // compiler hasn't freed the last retired plugin, so wait one callback
    if (retired_.load())
    {
        return;
    }


    Plugin *next = pending_.exchange(nullptr);
    if (!next)
    {
        return;
    }

    PluginPtr old = std::move(active_);
    active_.reset(next);
    if (old)
    {
        retired_.store(old.release());
    }
}

int AudioEngine::callback(
    const void* input,
    void* output,
    unsigned long frames,
    const PaStreamCallbackTimeInfo*,
    PaStreamCallbackFlags,
    void* user)
{
    auto *self = static_cast<AudioEngine*>(user);
    self->swap();

    const auto* in  = static_cast<const float*>(input);
    auto*       out = static_cast<float*>(output);

    // why do we do this here we could put this static or soemthign
    auto clamp = [](float v)
    {
        return std::clamp(v, -1.0f, 1.0f);
    };

    Plugin* p = self->active_.get();

    if (!p)
    {
        for (unsigned long i = 0; i < frames; i++)
        {
            float s = in ? clamp(in[i]) : 0.0f;
            out[i * 2] = out[i * 2 + 1] = s;
        }
        return paContinue;
    }

    for (unsigned long i = 0; i < frames; i++)
    {
        float x = in ? in[i] : 0.0f;
        float l = x, r = x;

        if (p->stereo())
        {
            Stereo o = p->stereo()({x, x});
            l = o.left;
            r = o.right;
        }
        else if (p->mono())
        {
            l = r = p->mono()(x);
        }

        // nan and stuff like taht
        if (!std::isfinite(l)) l = 0.0f;
        if (!std::isfinite(r)) r = 0.0f;

        // cclamp between 1 and -1
        out[i * 2]     = clamp(l);
        out[i * 2 + 1] = clamp(r);
    }
    return paContinue;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
AudioEngine::~AudioEngine()
{
    stop();
}

bool AudioEngine::start()
{
    // return if we couldnt init
    if (Pa_Initialize() != paNoError) return false;

    auto in_dev  = Pa_GetDefaultInputDevice();
    auto out_dev = Pa_GetDefaultOutputDevice();
    auto* in_info  = Pa_GetDeviceInfo(in_dev);
    auto* out_info = Pa_GetDeviceInfo(out_dev);
    if (!in_info || !out_info)
    {
        fprintf(stderr, "no audio device\n");
        Pa_Terminate();
        return false;
    }

    HostState::sample_rate.store(out_info->defaultSampleRate);

    PaStreamParameters ip{}, op{};
    ip.device = in_dev;
    ip.channelCount = 1;
    ip.sampleFormat = paFloat32;
    ip.suggestedLatency = in_info->defaultLowInputLatency;

    op.device = out_dev;
    op.channelCount = 2;
    op.sampleFormat = paFloat32;
    op.suggestedLatency = out_info->defaultLowOutputLatency;

    if (Pa_OpenStream(&stream_, &ip, &op, HostState::sample_rate.load(), 64,
                      paClipOff, &AudioEngine::callback, this) != paNoError ||
        Pa_StartStream(stream_) != paNoError)
    {
        fprintf(stderr, "could not start audio\n");
        Pa_Terminate();
        stream_ = nullptr;
        return false;
    }
    return true;
}

bool AudioEngine::stop()
{
    if (!stream_) return true;

    PaError e = Pa_StopStream(stream_);
    if (e != paNoError && e != paStreamIsStopped) return false;

    Pa_CloseStream(stream_);
    stream_ = nullptr;

    // no callback can run now we OWN every slot
    active_.reset();
    delete pending_.exchange(nullptr);
    delete retired_.exchange(nullptr);

    Pa_Terminate();
    return true;
}

void AudioEngine::publish(PluginPtr p)
{
    Plugin* raw = p.release();

    // destroy older unconsumed builds
    delete pending_.exchange(raw);
}

void AudioEngine::reclaim()
{
    // please reclaim
    delete retired_.exchange(nullptr);
}
