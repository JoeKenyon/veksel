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

    bool logContains(const veksel::Log& log, const std::string& text, std::chrono::milliseconds timeout = 1000ms)
    {
        for (auto waited = 0ms; waited < timeout; waited += 5ms)
        {
            for (const auto& line : log.getLines())
            {
                if (line.find(text) != std::string::npos)
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

TEST_CASE("Engine handles 64-bit integer <-> float conversions", "[compile]")
{
    veksel::Log log;
    veksel::Engine engine{log};

    // every cast here makes tcc emit a call to a runtime helper on x86_64
    // (__fixsfdi, __fixunssfdi, __floatundidf, __floatundisf), which Builtins.cpp provides
    engine.submitSource(
        "float process_sample(float input)\n"
        "{\n"
        "    unsigned long long u = (unsigned long long)(input * 1000.0f);\n"  // float -> u64
        "    long long          i = (long long)(input * -1000.0f);\n"          // float -> i64
        "    double             d = (double)u;\n"                              // u64   -> double
        "    float              f = (float)(u + 1ull);\n"                      // u64   -> float
        "    return f + (float)d + (float)i;\n"
        "}\n");

    // input 2: u = 2000, i = -2000, d = 2000, f = 2001  ->  2001
    REQUIRE(waitForSample(engine, 2.0f, [](float y) { return y == 2001.0f; }));
}

TEST_CASE("Engine handles unsigned 64-bit values above INT64_MAX", "[compile]")
{
    veksel::Log log;
    veksel::Engine engine{log};

    engine.submitSource(
        "float process_sample(float input)\n"
        "{\n"
        "    unsigned long long big = (1ull << 63) + (unsigned long long)input;\n"
        "    float f = (float)big;\n"                                           // u64   -> float, top bit set
        "    unsigned long long back = (unsigned long long)f;\n"                // float -> u64,   top bit set
        "    return back == (1ull << 63) ? 1.0f : 0.0f;\n"
        "}\n");

    // input 2 would come back as 2 under passthrough, so 1 proves the plugin ran and was correct
    REQUIRE(waitForSample(engine, 2.0f, [](float y) { return y == 1.0f; }));
}

TEST_CASE("Engine only exposes the builtin functions", "[compile]")
{
    veksel::Log log;
    veksel::Engine engine{log};

    engine.submitSource("float process_sample(float input) { printf(\"hi\"); return input; }");

    CHECK(logContains(log, "printf"));
    engine.beginBlock();
    CHECK(engine.processSample(1.0f) == 1.0f);
}