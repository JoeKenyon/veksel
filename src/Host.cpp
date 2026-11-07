#include "Host.h"
#include "HostState.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sys/select.h>
#include <unistd.h>

namespace fs = std::filesystem;

static std::string slurp(const fs::path& p)
{
    std::ifstream f(p);
    return std::string((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
}

Host::Host() = default;

int Host::run()
{
    setvbuf(stdout, nullptr, _IOLBF, 0);
    HostState::reset_defaults();

    if (!engine_.start())
    {
        return 1;
    }

    compiler_.start();
    printf("commands: k1 0.8 (set knob 1) / q (quit)\n");

    std::ofstream("plugin.c", std::ios::app);

    while (!quit_)
    {
        poll_terminal();
        watch_file();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    shutdown();
    return 0;
}

void Host::poll_terminal()
{
    fd_set r;
    timeval tv = {0, 0};

    FD_ZERO(&r);
    FD_SET(0, &r);

    if (!have_stdin_ || select(1, &r, nullptr, nullptr, &tv) <= 0)
    {
        return;
    }

    char buf[256];
    ssize_t n = read(0, buf, sizeof buf);
    if (n <= 0)
    {
        have_stdin_ = false; return;
    }

    stdin_buf_.append(buf, static_cast<size_t>(n));
    size_t nl;
    while ((nl = stdin_buf_.find('\n')) != std::string::npos)
    {
        handle_line(stdin_buf_.substr(0, nl));
        stdin_buf_.erase(0, nl + 1);
    }

    if (stdin_buf_.size() > 1024)
    {
        stdin_buf_.clear();
    }
}

void Host::handle_line(const std::string &line)
{
    if (line == "q")
    {
        quit_ = true; return;
    }

    int i = 0; float v = 0;
    if (sscanf(line.c_str(), "k%d %f", &i, &v) == 2 && i >= 1 && i <= 12)
    {
        HostState::knobs[i - 1].store(std::clamp(v, 0.0f, 1.0f));
        printf("knob %d = %.2f\n", i, v);
    }
}

void Host::watch_file()
{
    std::error_code ec;
    auto t = fs::last_write_time("plugin.c", ec);

    if (ec || t == last_mtime_)
    {
        return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    last_mtime_ = fs::last_write_time("plugin.c", ec);

    if (!ec)
    {
        compiler_.submit(slurp("plugin.c"));
    }
}

void Host::shutdown()
{
    compiler_.stop();

    if (!engine_.stop())
    {
        fprintf(stderr, "could not stop audio, exiting without cleanup\n");
        _exit(1);
    }
}
