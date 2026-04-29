#pragma once

#include <functional>
#include <string>
#include <string_view>

namespace GameCore::Core
{
    enum class LogLevel
    {
        Debug,
        Info,
        Warning,
        Error,
    };

    class Diagnostics
    {
    public:
        using Sink = std::function<void(LogLevel, std::string_view)>;

        Diagnostics();

        void setSink(Sink sink);

        void log(LogLevel level, std::string_view message) const;
        void debug(std::string_view message) const;
        void info(std::string_view message) const;
        void warning(std::string_view message) const;
        void error(std::string_view message) const;

    private:
        Sink m_sink;
    };
}
