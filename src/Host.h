#pragma once

#include "AudioEngine.h"
#include "Compiler.h"
#include <filesystem>
#include <string>

// Main thread: wires audio + compiler together, watches plugin.c, reads
// terminal commands. This is the class you'd replace with a GUI.
class Host
{
public:
    Host();
    int run();

private:
    void poll_terminal();
    void watch_file();
    void handle_line(const std::string& line);
    void shutdown();

    AudioEngine engine_;
    Compiler    compiler_{engine_};

    std::string stdin_buf_;
    bool        have_stdin_ = true;
    bool        quit_       = false;
    std::filesystem::file_time_type last_mtime_ = std::filesystem::file_time_type::min();
};
