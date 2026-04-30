#include "GameCore/Core/Diagnostics.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <utility>

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
    {
        addSink([](LogLevel level, const std::string_view message) {
            auto& output = level == LogLevel::Warning || level == LogLevel::Error ? std::cerr : std::cout;
            output << toPrefix(level) << " " << message << '\n';
        });
    }

    Diagnostics::SinkID Diagnostics::addSink(Sink sink)
    {
        if (!sink)
        {
            throw std::invalid_argument("Diagnostics sink cannot be empty.");
        }

        const SinkID id = m_nextSinkID++;
        m_sinks.push_back(SinkRecord{
            id,
            std::move(sink),
        });
        return id;
    }

    void Diagnostics::removeSink(const SinkID id)
    {
        m_sinks.erase(
            std::remove_if(m_sinks.begin(),
                           m_sinks.end(),
                           [id](const SinkRecord& sink) {
                               return sink.id == id;
                           }),
            m_sinks.end());
    }

    void Diagnostics::clearSinks()
    {
        m_sinks.clear();
    }

    void Diagnostics::setSink(Sink sink)
    {
        clearSinks();
        addSink(std::move(sink));
    }

    void Diagnostics::setMinimumLevel(const LogLevel level)
    {
        m_minimumLevel = level;
    }

    void Diagnostics::log(const LogLevel level, const std::string_view message) const
    {
        if (static_cast<int>(level) < static_cast<int>(m_minimumLevel))
        {
            return;
        }

        for (const auto& sink : m_sinks)
        {
            sink.sink(level, message);
        }
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

    LogLevel Diagnostics::minimumLevel() const
    {
        return m_minimumLevel;
    }

    std::size_t Diagnostics::sinkCount() const
    {
        return m_sinks.size();
    }
}
