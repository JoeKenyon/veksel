#include <catch2/catch_test_macros.hpp>
#include "Log.h"

#include <thread>
#include <vector>

// make sure logs stay in the exact order we push them in
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

// verify that clearing actually wipes out everything
TEST_CASE("Log clear empties", "[log]")
{
    veksel::Log log;
    log.write("x");
    log.clear();
    CHECK(log.getLines().empty());
}

// use the log with multiple threads at once to check for race conditions
TEST_CASE("Log is thread safe", "[log]")
{
    veksel::Log log;
    constexpr int perThread = 1000;
    constexpr int threads = 4;

    std::vector<std::thread> workers;
    for (int t = 0; t < threads; ++t)
    {
        workers.emplace_back([&log, t] 
        {
            for (int i = 0; i < perThread; ++i)
            {
                log.write("t" + std::to_string(t) + " " + std::to_string(i));
            }
        });
    }

    for (auto& w : workers) 
    {
        w.join();
    }
    
    CHECK(log.getLines().size() == static_cast<size_t>(threads * perThread));
}