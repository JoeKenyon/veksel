
#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace veksel
{
    class Log
    {
    public:
        // called whenever a new message is written
        using Callback = std::function<void(const std::string&)>;

        Log() = default;
        ~Log() = default;

        // store a message and notify the ui
        void write(std::string message);

        // return a copy of all log messages
        std::vector<std::string> getLines() const;

        // remove all stored messages
        void clear();

        // set the callback used to send messages to the ui
        void setCallback(Callback callback);

    private:
        // protect the messages and callback from other threads
        mutable std::mutex m_mutex;

        std::vector<std::string> m_lines;
        Callback m_callback;
    };
}
