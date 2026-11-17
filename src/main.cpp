#include "Audio.h"
#include "VekselEngine.h"
#include <csignal>
#include "platform.h"
#include "UI.h"

// flips when we need to bail out, main loop checks it every frame.
static volatile std::sig_atomic_t quit = 0;

// ctrl+c in the terminal, just set the flag and let main clean up properly
static void on_sigint(int)
{
    quit = 1;
}

int main()
{
    // catch ctrl+c
    std::signal(SIGINT, on_sigint);

    // initialize window and opengl context via platform layer
    if (!platform_init("Veksel", 900, 800))
        return 1;

    // engine first so it outlives the audio stream
    veksel::Engine engine;
    veksel::Audio audio;

    // start the default portaudio stream, passing the engine for the callback
    if (!audio.start(engine))
    {
        platform_shutdown();
        return 1;
    }

    // compiler thread
    engine.start();

    // pass both engine and audio references to UI
    veksel::UI ui(engine, audio);

    // main application loop
    while (!quit && platform_begin_frame())
    {
        // swap in any freshly compiled plugin code safely
        engine.update();

        ui.render();

        platform_end_frame();
    }

    // clean shutdown.
    // the audio stream must be stopped before the engine is destroyed so the
    // portaudio callback can never run processSample() on a dead engine.
    audio.shutdown();
    engine.stop();

    platform_shutdown();

    return 0;
}
