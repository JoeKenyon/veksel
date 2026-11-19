#include <catch2/catch_test_macros.hpp>
#include "Compiler.h"
#include "Log.h"

#include <chrono>
#include <memory>
#include <thread>

using namespace std::chrono_literals;

// poll the compiler until we get a plugin back or time out
static std::unique_ptr<veksel::Plugin> waitFor(veksel::Compiler& c,
                                               std::chrono::milliseconds timeout = 3000ms)
{
    for (auto waited = 0ms; waited < timeout; waited += 5ms)
    {
        if (auto p = c.take())
            return p;
        std::this_thread::sleep_for(5ms);
    }
    return nullptr;
}

// valid code compiles and runs
TEST_CASE("Compiler produces a working plugin", "[compiler]")
{
    veksel::Log log;
    veksel::Compiler compiler{log};

    compiler.submit("veksel_main { return input * 0.5f; }");

    auto plugin = waitFor(compiler);
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

    CHECK(waitFor(compiler, 500ms) == nullptr);

    bool sawError = false;
    for (const auto& line : log.getLines())
        if (line.find("[error]") != std::string::npos)
            sawError = true;
    CHECK(sawError);
}

// code without veksel_main is dropped
TEST_CASE("Compiler rejects source with no entry point", "[compiler]")
{
    veksel::Log log;
    veksel::Compiler compiler{log};

    compiler.submit("int main() { return 0; }");

    CHECK(waitFor(compiler, 500ms) == nullptr);
}

// math functions work through the prelude
TEST_CASE("Compiler handles math functions from the prelude", "[compiler]")
{
    veksel::Log log;
    veksel::Compiler compiler{log};

    compiler.submit("veksel_main { return sinf(input); }");

    auto plugin = waitFor(compiler);
    REQUIRE(plugin != nullptr);
    CHECK(plugin->function(0.0f) == 0.0f);
    CHECK(plugin->function(1.57079632679f) > 0.99f);
}