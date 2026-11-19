#include <catch2/catch_test_macros.hpp>
#include "Log.h"

#include <string>
#include <thread>
#include <vector>

// logs stay in the order we write them
TEST_CASE("Log stores lines in order", "[log]")
{
    veksel::Log log;
    log.write("first");
    log.write("second");

    const auto lines = log.getLines();
    REQUIRE(lines.size() == 2);
    CHECK(lines[0] == "first");
    CHECK(lines[1] == "second");
}

// clearing wipes everything
TEST_CASE("Log clear empties", "[log]")
{
    veksel::Log log;
    log.write("x");
    log.clear();
    CHECK(log.getLines().empty());
}

// the log keeps the newest MaxLines and drops the oldest
TEST_CASE("Log is capped", "[log]")
{
    veksel::Log log;
    const size_t total = veksel::Log::MaxLines + 50;

    for (size_t i = 0; i < total; ++i)
        log.write(std::to_string(i));

    const auto lines = log.getLines();
    REQUIRE(lines.size() == veksel::Log::MaxLines);
    CHECK(lines.front() == "50");
    CHECK(lines.back() == std::to_string(total - 1));
}

// many threads writing at once lose nothing (total stays under the cap)
TEST_CASE("Log is thread safe", "[log]")
{
    veksel::Log log;
    constexpr int perThread = 100;
    constexpr int threads = 4;
    static_assert(perThread * threads <= veksel::Log::MaxLines);

    std::vector<std::thread> workers;
    for (int t = 0; t < threads; ++t)
    {
        workers.emplace_back([&log, t]
        {
            for (int i = 0; i < perThread; ++i)
                log.write("t" + std::to_string(t) + " " + std::to_string(i));
        });
    }

    for (auto& w : workers)
        w.join();

    CHECK(log.getLines().size() == static_cast<size_t>(threads * perThread));
}