#include "GameCore/Core/Diagnostics.h"

#include <iostream>

namespace GameCore::Core
{
    namespace
    {
        constexpr std::string_view toPrefix(LogLevel level)
        {
            switch (level)
            {
            case LogLevel::Debug:
                return "[Debug]";
            case LogLevel::Info:
                return "[Info]";
            case LogLevel::Warning:
                return "[Warning]";
            case LogLevel::Error:
                return "[Error]";
            }

            return "[Unknown]";
        }
    }

    Diagnostics::Diagnostics()
        : m_sink([](LogLevel level, const std::string_view message) {
            std::cout << toPrefix(level) << " " << message << '\n';
        })
    {
    }

    void Diagnostics::setSink(Sink sink)
    {
        if (sink)
        {
            m_sink = std::move(sink);
        }
    }

    void Diagnostics::log(const LogLevel level, const std::string_view message) const
    {
        m_sink(level, message);
    }

    void Diagnostics::debug(const std::string_view message) const
    {
        log(LogLevel::Debug, message);
    }

    void Diagnostics::info(const std::string_view message) const
    {
        log(LogLevel::Info, message);
    }

    void Diagnostics::warning(const std::string_view message) const
    {
        log(LogLevel::Warning, message);
    }

    void Diagnostics::error(const std::string_view message) const
    {
        log(LogLevel::Error, message);
    }
}
