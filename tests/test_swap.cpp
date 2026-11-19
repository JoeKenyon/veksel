#include <catch2/catch_test_macros.hpp>
#include "Engine.h"
#include "Log.h"

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

// act as ui and audio thread together until probe gives expect, or time out
static bool driveUntil(veksel::Engine& engine, float probe, float expect,
                       std::chrono::milliseconds timeout = 3000ms)
{
    for (auto waited = 0ms; waited < timeout; waited += 5ms)
    {
        engine.update();       // ui: hand over new plugin, delete old one
        engine.beginBlock();   // audio: pick it up

        if (engine.processSample(probe) == expect)
            return true;

        std::this_thread::sleep_for(5ms);
    }
    return false;
}

// before any source is loaded the engine is a passthrough
TEST_CASE("no plugin: passthrough", "[swap]")
{
    veksel::Log log;
    veksel::Engine engine{log};

    engine.beginBlock();
    CHECK(engine.processSample(0.25f) == 0.25f);
    CHECK(engine.processSample(-0.5f) == -0.5f);
}

// a submitted plugin goes live
TEST_CASE("first plugin installs", "[swap]")
{
    veksel::Log log;
    veksel::Engine engine{log};

    engine.submitSource("veksel_main { return input * 2.0f; }");

    REQUIRE(driveUntil(engine, 1.0f, 2.0f));
    CHECK(engine.processSample(3.0f) == 6.0f);
}

// swapping from one plugin to another
TEST_CASE("swapping A -> B replaces the function", "[swap]")
{
    veksel::Log log;
    veksel::Engine engine{log};

    engine.submitSource("veksel_main { return input + 1.0f; }");
    REQUIRE(driveUntil(engine, 0.0f, 1.0f));

    engine.submitSource("veksel_main { return input * 10.0f; }");
    REQUIRE(driveUntil(engine, 2.0f, 20.0f));

    CHECK(engine.processSample(0.5f) == 5.0f);
}

// a plugin never changes in the middle of a block
TEST_CASE("plugin only changes at beginBlock", "[swap]")
{
    veksel::Log log;
    veksel::Engine engine{log};

    engine.submitSource("veksel_main { return input + 1.0f; }");
    REQUIRE(driveUntil(engine, 0.0f, 1.0f));

    engine.submitSource("veksel_main { return input * 10.0f; }");

    // ui hands the new plugin over, but the audio thread hasn't started a block
    for (int i = 0; i < 600; ++i)
    {
        engine.update();
        std::this_thread::sleep_for(5ms);
        CHECK(engine.processSample(2.0f) == 3.0f);   // still the old one
    }
}