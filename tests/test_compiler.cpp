#include <catch2/catch_test_macros.hpp>
#include "Engine.h"
#include "Log.h"

#include <chrono>
#include <string>
#include <thread>

using namespace std::chrono_literals;

namespace
{
    bool hasErrorLog(const veksel::Log& log, std::chrono::milliseconds timeout = 1000ms)
    {
        for (auto waited = 0ms; waited < timeout; waited += 5ms)
        {
            for (const auto& line : log.getLines())
            {
                if (line.find("[error]") != std::string::npos)
                    return true;
            }
            std::this_thread::sleep_for(5ms);
        }
        return false;
    }

    template <typename Pred>
    bool waitForSample(veksel::Engine& engine, float input, Pred&& pred, std::chrono::milliseconds timeout = 3000ms)
    {
        for (auto waited = 0ms; waited < timeout; waited += 5ms)
        {
            engine.beginBlock();
            if (pred(engine.processSample(input)))
                return true;

            std::this_thread::sleep_for(5ms);
        }
        return false;
    }
}

TEST_CASE("Engine compiles and runs a valid plugin", "[compile]")
{
    veksel::Log log;
    veksel::Engine engine{log};

    engine.submitSource("float process_sample(float input) { return input * 0.5f; }");

    REQUIRE(waitForSample(engine, 1.0f, [](float y) { return y == 0.5f; }));
}

TEST_CASE("Engine rejects broken source", "[compile]")
{
    veksel::Log log;
    veksel::Engine engine{log};

    engine.submitSource("float process_sample(float input) { return nope; }");

    CHECK(hasErrorLog(log));
    engine.beginBlock();
    CHECK(engine.processSample(1.0f) == 1.0f);
}

TEST_CASE("Engine keeps previous plugin when new compile fails", "[compile]")
{
    veksel::Log log;
    veksel::Engine engine{log};

    engine.submitSource("float process_sample(float input) { return input * 2.0f; }");
    REQUIRE(waitForSample(engine, 1.0f, [](float y) { return y == 2.0f; }));

    engine.submitSource("float process_sample(float input) { return broken + ; }");
    REQUIRE(hasErrorLog(log));

    engine.beginBlock();
    CHECK(engine.processSample(1.0f) == 2.0f);
}

TEST_CASE("Engine rejects source with no entry point", "[compile]")
{
    veksel::Log log;
    veksel::Engine engine{log};

    engine.submitSource("int main() { return 0; }");

    CHECK(hasErrorLog(log));
    engine.beginBlock();
    CHECK(engine.processSample(1.0f) == 1.0f);
}

TEST_CASE("Engine handles math functions from the prelude", "[compile]")
{
    veksel::Log log;
    veksel::Engine engine{log};

    engine.submitSource("float process_sample(float input) { return sinf(input); }");

    // 1.570796... under passthrough would be > 0.99, so bound near 1.0
    REQUIRE(waitForSample(engine, 1.57079632679f, [](float y) {
        return y > 0.99f && y < 1.01f;
    }));
    CHECK(engine.processSample(0.0f) == 0.0f);
}