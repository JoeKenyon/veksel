#include "VekselEngine.h"

#include <algorithm>
#include <cmath>
#include <csignal>
#include <atomic>
#include "platform.h"
#include "portaudio.h"
#include "UI.h"

// flips when we need to bail out, main loop checks it every frame
static std::atomic<bool> quit{false};

// ctrl+c in the terminal, just set the flag and let main clean up properly
static void on_sigint(int)
{
    quit = true;
}

// portaudio callback
// runs on the audio thread so nothing slow in here, no allocs, no locks
static int callback(
    const void* input,
    void* output,
    unsigned long frames,
    const PaStreamCallbackTimeInfo*,
    PaStreamCallbackFlags,
    void* user_data)
{
    // engine was handed over when the stream opened, just cast it back
    auto* engine = static_cast<veksel::Engine*>(user_data);

    const float* in = static_cast<const float*>(input);
    float* out = static_cast<float*>(output);

    // one sample at a time, simple but fine for now
    for (unsigned long i = 0; i < frames; ++i)
    {
        // no input device -> just feed it silence
        float sample = in ? in[i] : 0.0f;

        // whatever the user compiled does its thing here
        sample = engine->process_sample(sample);

        // nan or inf from a bad plugin would blow up the speakers, so zero it
        if (!std::isfinite(sample))
            sample = 0.0f;

        // hard clamp so nothing goes past full scale
        sample = std::clamp(sample, -1.0f, 1.0f);

        // mono in, so same sample to left and right
        out[i * 2] = sample;
        out[i * 2 + 1] = sample;
    }

    return paContinue;
}

int main()
{
    // catch ctrl+c
    std::signal(SIGINT, on_sigint);

    // engine first so it outlives the audio stream
    veksel::Engine engine;

    // window and opengl
    if (!platform_init("Veksel", 900, 800))
        return 1;

    // portaudio
    if (Pa_Initialize() != paNoError)
    {
        platform_shutdown();
        return 1;
    }

    // input, mono
    PaStreamParameters in_params = {};
    in_params.device = Pa_GetDefaultInputDevice();
    in_params.channelCount = 1;
    in_params.sampleFormat = paFloat32;
    in_params.suggestedLatency = 0.01;

    // output, stereo
    PaStreamParameters out_params = {};
    out_params.device = Pa_GetDefaultOutputDevice();
    out_params.channelCount = 2;
    out_params.sampleFormat = paFloat32;
    out_params.suggestedLatency = 0.01;

    PaStream* stream = nullptr;

    // open the stream, engine goes in as user data so the callback can reach it
    // 44100 and 64 frames hardcoded until settings are wired up
    if (Pa_OpenStream(
            &stream,
            &in_params,
            &out_params,
            44100,
            64,
            paClipOff,
            callback,
            &engine) != paNoError)
    {
        Pa_Terminate();
        platform_shutdown();
        return 1;
    }

    // compiler thread
    engine.start();

    UI ui(engine);

    // audio goes live
    if (Pa_StartStream(stream) != paNoError)
    {
        Pa_CloseStream(stream);
        engine.stop();
        Pa_Terminate();
        platform_shutdown();
        return 1;
    }

    // main ui loop
    while (!quit && platform_begin_frame())
    {
        // swap in any freshly compiled plugin
        engine.update();

        ui.render();

        platform_end_frame();
    }

    // audio has to stop first so the callback isn't touching the plugin
    // while the engine is being torn down
    PaError error = Pa_StopStream(stream);

    if (error != paNoError && error != paStreamIsStopped)
    {
        // todo: do something with this
    }

    Pa_CloseStream(stream);

    // join the compiler thread
    engine.stop();

    // shut everything else down
    Pa_Terminate();
    platform_shutdown();

    return 0;
}
