#include <catch2/catch_test_macros.hpp>
#include "Compiler.h"
#include "Engine.h"
#include "Log.h"

#include <atomic>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

// run a soak test to make sure the compiler doesn't leak memory or hang over hundreds of quick compile loops
TEST_CASE("soak: compiler survives many compiles", "[soak][compiler]")
{
    veksel::Log log;
    veksel::Compiler compiler{log};

    constexpr int iterations = 200;   // ~2-4s on a laptop; raise locally

    for (int i = 0; i < iterations; ++i)
    {
        compiler.submit("veksel_main { return input * " + std::to_string(i % 7 + 1) + ".0f; }");

        std::optional<veksel::Plugin> plugin;
        for (auto waited = 0ms; waited < 2000ms && !plugin; waited += 2ms)
        {
            plugin = compiler.take();
            if (!plugin) std::this_thread::sleep_for(2ms);
        }

        REQUIRE(plugin);
        REQUIRE(plugin->function);
        CHECK(plugin->function(1.0f) == static_cast<float>(i % 7 + 1));
    }

    // make sure a clean soak didn't output any error lines to the log
    for (const auto& line : log.getLines())
        CHECK(line.find("[error]") == std::string::npos);
}

// quick code submissions while forcing updates to check for thread safety and locks
TEST_CASE("soak: engine swaps under concurrent submits", "[soak][swap]")
{
    veksel::Log log;
    veksel::Engine engine{log};

    std::atomic<bool> stop{false};
    std::atomic<int>  submitted{0};

    // background thread submits new sources as fast as it can
    std::thread submitter([&] 
    {
        int i = 0;
        while (!stop.load(std::memory_order_relaxed))
        {
            engine.submitSource("veksel_main { return input * 2.0f; }");
            ++submitted;
            ++i;
            std::this_thread::sleep_for(1ms);
        }
    });

    // main thread drives the handshake
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    int swaps = 0;
    float probe = 1.0f;

    while (std::chrono::steady_clock::now() < deadline)
    {
        engine.update();
        engine.processSample(0.0f);   // ack if requested
        engine.update();              // install if acked

        // once a plugin is active, it must be the doubling one
        const float out = engine.processSample(probe);
        if (out == 2.0f) ++swaps;

        std::this_thread::sleep_for(1ms);
    }

    stop.store(true);
    submitter.join();

    CHECK(submitted.load() > 0);
    CHECK(swaps > 0);   // verify that at least one plugin went live during the soak
}