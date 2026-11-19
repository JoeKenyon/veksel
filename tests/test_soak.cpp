#include <catch2/catch_test_macros.hpp>
#include "Compiler.h"
#include "Engine.h"
#include "Log.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

using namespace std::chrono_literals;

// many quick compiles don't leak, hang or log errors
TEST_CASE("soak: compiler survives many compiles", "[soak][compiler]")
{
    veksel::Log log;
    veksel::Compiler compiler{log};

    constexpr int iterations = 200;

    for (int i = 0; i < iterations; ++i)
    {
        compiler.submit("veksel_main { return input * " + std::to_string(i % 7 + 1) + ".0f; }");

        std::unique_ptr<veksel::Plugin> plugin;
        for (auto waited = 0ms; waited < 2000ms && !plugin; waited += 2ms)
        {
            plugin = compiler.take();
            if (!plugin)
                std::this_thread::sleep_for(2ms);
        }

        REQUIRE(plugin != nullptr);
        REQUIRE(plugin->function != nullptr);
        CHECK(plugin->function(1.0f) == static_cast<float>(i % 7 + 1));
    }

    for (const auto& line : log.getLines())
        CHECK(line.find("[error]") == std::string::npos);
}

// a real audio thread runs while the ui thread keeps swapping plugins
TEST_CASE("soak: engine swaps under a running audio thread", "[soak][swap]")
{
    veksel::Log log;
    veksel::Engine engine{log};

    std::atomic<bool> stop{false};
    std::atomic<long> badSamples{0};
    std::atomic<long> doubled{0};
    std::atomic<long> tripled{0};

    // audio thread: blocks of 64 samples, probe 1.0.
    // passthrough gives 1, the plugins give 2 or 3. anything else is a bug.
    // a block must never mix two different plugins
    std::thread audio([&]
    {
        while (!stop.load(std::memory_order_relaxed))
        {
            engine.beginBlock();

            const float first = engine.processSample(1.0f);
            bool mixed = false;

            for (int i = 1; i < 64; ++i)
                if (engine.processSample(1.0f) != first)
                    mixed = true;

            if (mixed || (first != 1.0f && first != 2.0f && first != 3.0f))
                ++badSamples;
            else if (first == 2.0f)
                ++doubled;
            else if (first == 3.0f)
                ++tripled;

            std::this_thread::sleep_for(100us);
        }
    });

    // submitter thread: alternate between two plugins as fast as it can
    std::thread submitter([&]
    {
        bool two = true;
        while (!stop.load(std::memory_order_relaxed))
        {
            engine.submitSource(two ? "veksel_main { return input * 2.0f; }"
                                    : "veksel_main { return input * 3.0f; }");
            two = !two;
            std::this_thread::sleep_for(1ms);
        }
    });

    // this thread is the ui thread
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < deadline)
    {
        engine.update();
        std::this_thread::sleep_for(1ms);
    }

    stop.store(true);
    submitter.join();
    audio.join();

    CHECK(badSamples.load() == 0);
    CHECK(doubled.load() > 0);
    CHECK(tripled.load() > 0);
}