#pragma once

#include <mutex>
#include <string>
#include <deque>


namespace veksel
{
    // thread-safe list of log lines. the ui polls getLines()
    class Log
    {
    public:
        static constexpr size_t MaxLines = 500;

        void write(std::string message);

        // copy of the stored lines, oldest first
        std::deque<std::string> getLines() const;

        // remove all stored lines
        void clear();

    private:
        mutable std::mutex m_mutex;
        std::deque<std::string> m_lines;
    };
}