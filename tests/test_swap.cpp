#include <catch2/catch_test_macros.hpp>
#include "Engine.h"
#include "Log.h"

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

// keep driving the engine update loop and processing samples until our code swap takes effect or we time out
static bool driveSwapUntilActive(veksel::Engine& engine, float probe, float expect, std::chrono::milliseconds timeout = 3000ms)
{
    for (auto waited = 0ms; waited < timeout; waited += 5ms)
    {
        engine.update();
        engine.processSample(0.0f);
        engine.update();
        if (engine.processSample(probe) == expect)
        {
            return true;
        }
        std::this_thread::sleep_for(5ms);
    }
    return false;
}

// verify that before any source code is loaded, the engine just acts as a clean passthrough
TEST_CASE("no plugin: passthrough", "[swap]")
{
    veksel::Log log;
    veksel::Engine engine{log};

    CHECK(engine.processSample(0.25f) == 0.25f);
    CHECK(engine.processSample(-0.5f) == -0.5f);
}

// check that submitting new source code compiles and swaps in cleanly after the thread handshake
TEST_CASE("first plugin installs after handshake", "[swap]")
{
    veksel::Log log;
    veksel::Engine engine{log};

    engine.submitSource("veksel_main { return input * 2.0f; }");

    REQUIRE(driveSwapUntilActive(engine, 1.0f, 2.0f));
    CHECK(engine.processSample(3.0f) == 6.0f);
}

// test hot-swapping from one DSP function straight to another while running
TEST_CASE("swapping A -> B replaces the function", "[swap]")
{
    veksel::Log log;
    veksel::Engine engine{log};

    engine.submitSource("veksel_main { return input + 1.0f; }");
    REQUIRE(driveSwapUntilActive(engine, 0.0f, 1.0f));

    engine.submitSource("veksel_main { return input * 10.0f; }");
    REQUIRE(driveSwapUntilActive(engine, 2.0f, 20.0f));

    CHECK(engine.processSample(0.5f) == 5.0f);
}