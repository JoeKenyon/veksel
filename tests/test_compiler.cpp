#include <catch2/catch_test_macros.hpp>
#include "Compiler.h"
#include "Log.h"

#include <chrono>
#include <optional>
#include <thread>

using namespace std::chrono_literals;

// helper to poll the compiler queue until we get a plugin back or time out
static std::optional<veksel::Plugin> waitFor(veksel::Compiler& c,
                                            std::chrono::milliseconds timeout = 3000ms)
{
    for (auto waited = 0ms; waited < timeout; waited += 5ms)
    {
        if (auto p = c.take()) return p;
        std::this_thread::sleep_for(5ms);
    }
    return std::nullopt;
}

// make sure valid user code compiles and runs right
TEST_CASE("Compiler produces a working plugin", "[compiler]")
{
    veksel::Log log;
    veksel::Compiler compiler{log};

    compiler.submit("veksel_main { return input * 0.5f; }");

    auto plugin = waitFor(compiler);
    REQUIRE(plugin);
    REQUIRE(plugin->function);
    CHECK(plugin->function(1.0f) == 0.5f);
}

// check that syntax errors are caught and logged properly as errors
TEST_CASE("Compiler rejects broken source", "[compiler]")
{
    veksel::Log log;
    veksel::Compiler compiler{log};

    compiler.submit("veksel_main { return nope; }");

    auto plugin = waitFor(compiler, 500ms);
    CHECK_FALSE(plugin);

    bool sawError = false;
    for (const auto& line : log.getLines())
        if (line.find("[error]") != std::string::npos) { sawError = true; break; }
    CHECK(sawError);
}

// make sure code without our expected entry point gets dropped
TEST_CASE("Compiler rejects source with no entry point", "[compiler]")
{
    veksel::Log log;
    veksel::Compiler compiler{log};

    compiler.submit("int main() { return 0; }");

    auto plugin = waitFor(compiler, 500ms);
    CHECK_FALSE(plugin);
}

// test that standard math symbols work out of the box via the prelude
TEST_CASE("Compiler handles math functions from the prelude", "[compiler]")
{
    veksel::Log log;
    veksel::Compiler compiler{log};

    compiler.submit("veksel_main { return sinf(input); }");

    auto plugin = waitFor(compiler);
    REQUIRE(plugin);
    CHECK(plugin->function(0.0f) == 0.0f);
    CHECK(plugin->function(1.57079632679f) > 0.99f);
}