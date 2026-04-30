#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

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
        using SinkID = std::size_t;

        Diagnostics();

        SinkID addSink(Sink sink);
        void removeSink(SinkID id);
        void clearSinks();
        void setSink(Sink sink);
        void setMinimumLevel(LogLevel level);

        void log(LogLevel level, std::string_view message) const;
        void debug(std::string_view message) const;
        void info(std::string_view message) const;
        void warning(std::string_view message) const;
        void error(std::string_view message) const;

        [[nodiscard]] LogLevel minimumLevel() const;
        [[nodiscard]] std::size_t sinkCount() const;

    private:
        struct SinkRecord
        {
            SinkID id{0};
            Sink sink;
        };

        std::vector<SinkRecord> m_sinks;
        SinkID m_nextSinkID{1};
        LogLevel m_minimumLevel{LogLevel::Debug};
    };
}
