#include <catch2/catch_test_macros.hpp>
#include "Engine.h"
#include "Log.h"

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

namespace
{
    // step the audio block swap until processSample(probe) hits expect or times out
    bool driveUntil(
        veksel::Engine& engine,
        float probe,
        float expect,
        std::chrono::milliseconds timeout = 3000ms
    )
    {
        for (auto waited = 0ms; waited < timeout; waited += 5ms)
        {
            engine.beginBlock();

            if (engine.processSample(probe) == expect)
            {
                return true;
            }

            std::this_thread::sleep_for(5ms);
        }
        return false;
    }
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

    engine.submitSource("float process_sample(float input) { return input * 2.0f; }");

    REQUIRE(driveUntil(engine, 1.0f, 2.0f));
    CHECK(engine.processSample(3.0f) == 6.0f);
}

// swapping from one plugin to another
TEST_CASE("swapping A -> B replaces the function", "[swap]")
{
    veksel::Log log;
    veksel::Engine engine{log};

    engine.submitSource("float process_sample(float input) { return input + 1.0f; }");
    REQUIRE(driveUntil(engine, 0.0f, 1.0f));

    engine.submitSource("float process_sample(float input) { return input * 10.0f; }");
    REQUIRE(driveUntil(engine, 2.0f, 20.0f));

    CHECK(engine.processSample(0.5f) == 5.0f);
}

// a plugin never changes without beginBlock
TEST_CASE("plugin only changes at beginBlock", "[swap]")
{
    veksel::Log log;
    veksel::Engine engine{log};

    engine.submitSource("float process_sample(float input) { return input + 1.0f; }");
    REQUIRE(driveUntil(engine, 0.0f, 1.0f));

    engine.submitSource("float process_sample(float input) { return input * 10.0f; }");

    // give the compiler time to publish while skipping beginBlock
    for (int i = 0; i < 60; ++i)
    {
        std::this_thread::sleep_for(5ms);
        CHECK(engine.processSample(2.0f) == 3.0f);
    }

    // once beginBlock runs, the new plugin takes over
    engine.beginBlock();
    CHECK(engine.processSample(2.0f) == 20.0f);
}