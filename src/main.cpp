#include "Audio.h"
#include "Engine.h"
#include "Log.h"
#include "UI.h"
#include "Platform.h"

#include <csignal>

static volatile std::sig_atomic_t quit = 0;

static void on_sigint(int)
{
    quit = 1;
}

int main()
{
    std::signal(SIGINT, on_sigint);

    if (!platform_init("Veksel", 950, 800))
        return 1;

    // own scope so everything here (ui included) is destroyed before
    // platform_shutdown() tears down the imgui context
    {
        veksel::Log log;
        veksel::Engine engine{log};
        veksel::Audio audio{log};

        if (!audio.start(engine, -1, -1, 44100, 256))
            log.write("[error] Failed to start audio stream on startup.");

        veksel::UI ui{engine, audio, log};

        while (!quit && platform_begin_frame())
        {
            engine.update();

            ui.render();

            platform_end_frame();
        }

        audio.shutdown();
    }

    platform_shutdown();

    return 0;
}