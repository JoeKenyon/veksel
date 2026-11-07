#pragma once

#include "Plugin.h"
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

// Compiles source → .dylib → Plugin. Runs on its own thread.
// The audio engine is the only consumer of the output; this class
// just publishes plugins to it.
class Compiler
{
public:
    // engine: where compiled plugins go. Non-null.
    explicit Compiler(class AudioEngine &engine);
    ~Compiler();

    Compiler(const Compiler&)            = delete;
    Compiler& operator=(const Compiler&) = delete;

    void start();

    // newest wins; never blocks on a build
    void submit(std::string source);

    // aborts in-progress builds, joins
    void stop();

private:
    void loop();
    PluginPtr build(const std::string &source);
    void report(const std::string &msg);

    AudioEngine &engine_;
    std::mutex              mu_;
    std::condition_variable cv_;
    std::optional<std::string> code_;
    bool                    quit_ = false;
    std::thread             thread_;
};
