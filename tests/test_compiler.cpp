#include <catch2/catch_test_macros.hpp>
#include "Compiler.h"
#include "Log.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

using namespace std::chrono_literals;

namespace
{
    // pull a compiled plugin out of incoming() if one lands before timeout,
    // and hand ownership back to the caller (or test) to delete
    std::unique_ptr<veksel::Plugin> waitForIncoming(
        veksel::Compiler& c,
        std::chrono::milliseconds timeout = 3000ms
    )
    {
        for (auto waited = 0ms; waited < timeout; waited += 5ms)
        {
            if (veksel::Plugin* p = c.incoming().exchange(nullptr, std::memory_order_acq_rel))
            {
                return std::unique_ptr<veksel::Plugin>(p);
            }
            std::this_thread::sleep_for(5ms);
        }
        return nullptr;
    }

    bool hasErrorLog(const veksel::Log& log, std::chrono::milliseconds timeout = 500ms)
    {
        for (auto waited = 0ms; waited < timeout; waited += 5ms)
        {
            for (const auto& line : log.getLines())
            {
                if (line.find("[error]") != std::string::npos)
                {
                    return true;
                }
            }
            std::this_thread::sleep_for(5ms);
        }
        return false;
    }
}

// valid code compiles and runs
TEST_CASE("Compiler produces a working plugin", "[compiler]")
{
    veksel::Log log;
    veksel::Compiler compiler{log};

    compiler.submit("veksel_main { return input * 0.5f; }");

    auto plugin = waitForIncoming(compiler);
    REQUIRE(plugin != nullptr);
    REQUIRE(plugin->function != nullptr);
    CHECK(plugin->function(1.0f) == 0.5f);
}

// syntax errors produce no plugin and an error in the log
TEST_CASE("Compiler rejects broken source", "[compiler]")
{
    veksel::Log log;
    veksel::Compiler compiler{log};

    compiler.submit("veksel_main { return nope; }");

    CHECK(waitForIncoming(compiler, 500ms) == nullptr);
    CHECK(hasErrorLog(log));
}

// code without veksel_main is dropped
TEST_CASE("Compiler rejects source with no entry point", "[compiler]")
{
    veksel::Log log;
    veksel::Compiler compiler{log};

    compiler.submit("int main() { return 0; }");

    CHECK(waitForIncoming(compiler, 500ms) == nullptr);
    CHECK(hasErrorLog(log));
}

// math functions work through the prelude
TEST_CASE("Compiler handles math functions from the prelude", "[compiler]")
{
    veksel::Log log;
    veksel::Compiler compiler{log};

    compiler.submit("veksel_main { return sinf(input); }");

    auto plugin = waitForIncoming(compiler);
    REQUIRE(plugin != nullptr);
    CHECK(plugin->function(0.0f) == 0.0f);
    CHECK(plugin->function(1.57079632679f) > 0.99f);
}