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
        const float mult = static_cast<float>(i % 7 + 1);
        compiler.submit("veksel_main { return input * " + std::to_string(static_cast<int>(mult)) + ".0f; }");

        std::unique_ptr<veksel::Plugin> plugin;
        for (auto waited = 0ms; waited < 2000ms && !plugin; waited += 2ms)
        {
            if (veksel::Plugin* raw = compiler.incoming().exchange(nullptr, std::memory_order_acq_rel))
            {
                plugin.reset(raw);
            }
            else
            {
                std::this_thread::sleep_for(2ms);
            }
        }

        REQUIRE(plugin != nullptr);
        REQUIRE(plugin->function != nullptr);
        CHECK(plugin->function(1.0f) == mult);
    }

    for (const auto& line : log.getLines())
    {
        CHECK(line.find("[error]") == std::string::npos);
    }
}

// a simulated audio thread and submitter thread swap plugins concurrently
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
            {
                if (engine.processSample(1.0f) != first)
                {
                    mixed = true;
                }
            }

            if (mixed || (first != 1.0f && first != 2.0f && first != 3.0f))
            {
                ++badSamples;
            }
            else if (first == 2.0f)
            {
                ++doubled;
            }
            else if (first == 3.0f)
            {
                ++tripled;
            }

            std::this_thread::sleep_for(100us);
        }
    });

    // submitter thread: alternate between two plugins continuously
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

    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(10ms);
    }

    stop.store(true, std::memory_order_release);
    submitter.join();
    audio.join();

    CHECK(badSamples.load() == 0);
    CHECK(doubled.load() > 0);
    CHECK(tripled.load() > 0);
}

// repeated engine lifecycles with fast swaps, bad compiles and full teardown
TEST_CASE("no-sanitizer stress: swaps, invalid compiles, and teardown", "[soak][nosan]")
{
    constexpr int cycles = 20;

    for (int c = 0; c < cycles; ++c)
    {
        veksel::Log log;
        veksel::Engine engine{log};

        std::atomic<bool> stop{false};
        std::atomic<long> badSamples{0};
        std::atomic<long> blocks{0};
        std::atomic<long> sawTwo{0};
        std::atomic<long> sawThree{0};

        // simulated audio thread: never blocks, checks whole-block consistency
        std::thread audio([&]
        {
            while (!stop.load(std::memory_order_relaxed))
            {
                engine.beginBlock();

                const float first = engine.processSample(1.0f);
                bool mixed = false;

                for (int i = 1; i < 64; ++i)
                {
                    if (engine.processSample(1.0f) != first)
                    {
                        mixed = true;
                    }
                }

                if (mixed || (first != 1.0f && first != 2.0f && first != 3.0f))
                {
                    ++badSamples;
                }
                else if (first == 2.0f)
                {
                    ++sawTwo;
                }
                else if (first == 3.0f)
                {
                    ++sawThree;
                }

                ++blocks;
            }
        });

        // compiler thread feeder: alternates valid plugins and injects occasional bad code
        std::thread submitter([&]
        {
            int step = 0;
            while (!stop.load(std::memory_order_relaxed))
            {
                if (step % 7 == 0)
                {
                    engine.submitSource("veksel_main { return broken_symbol + ; }");
                }
                else if (step % 2 == 0)
                {
                    engine.submitSource("veksel_main { return input * 2.0f; }");
                }
                else
                {
                    engine.submitSource("veksel_main { return input * 3.0f; }");
                }

                ++step;
                std::this_thread::sleep_for(500us);
            }
        });

        std::this_thread::sleep_for(250ms);
        stop.store(true, std::memory_order_release);

        submitter.join();
        audio.join();

        CHECK(badSamples.load() == 0);
        CHECK(blocks.load() > 0);
        CHECK(sawTwo.load() > 0);
        CHECK(sawThree.load() > 0);
    }
}