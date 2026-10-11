//
// Created by Jake Rieger on 9/13/2026.
//

#pragma once

#include <array>
#include <fstream>
#include <mutex>

namespace Xen {
    class Logger;
    Logger& GetLogger();

    class Logger {
        friend Logger& GetLogger();
        Logger(const Logger&)            = delete;
        Logger(Logger&&)                 = delete;
        Logger& operator=(const Logger&) = delete;
        Logger& operator=(Logger&&)      = delete;

    public:
        static constexpr size_t LOGGER_MAX_ENTRIES {4096};

        enum class Severity : uint8_t {
            Info     = 0,
            Warning  = 1,
            Error    = 2,
            Critical = 3,
            Debug    = 4,
        };

        struct Entry {
            std::string Message;
            std::string TimeStamp;
            Severity Severity;
        };

        ~Logger();
        void Log(Severity Sev, const char* Msg);
        void Clear();

        template<typename... Args>
        void Log(const Severity Sev, const char* Fmt, Args... FmtArgs) {
            char Buffer[1024];
            std::snprintf(Buffer, sizeof(Buffer), Fmt, FmtArgs...);
            Log(Sev, Buffer);
        }

        std::mutex& GetBufferMutex() { return _BufferMutex; }
        size_t GetTotalEntries() const { return _TotalEntries; }
        size_t GetCurrentEntryIndex() const { return _CurrentEntry; }
        std::array<Entry, LOGGER_MAX_ENTRIES> const& GetEntries() const { return _Entries; }

    private:
        std::ofstream _LogStream;
        size_t _CurrentEntry {0};
        size_t _TotalEntries {0};
        std::array<Entry, LOGGER_MAX_ENTRIES> _Entries {};
        std::mutex _BufferMutex {};

        Logger();

        static std::string GetTimeStamp();
        static std::string GetSeverityString(Severity Sev);
        static std::string GetLogFileName();
    };
}  // namespace Xen

#define LOG_INFO(Msg, ...) Xen::GetLogger().Log(Xen::Logger::Severity::Info, Msg, ##__VA_ARGS__)
#define LOG_WARN(Msg, ...) Xen::GetLogger().Log(Xen::Logger::Severity::Warning, Msg, ##__VA_ARGS__)
#define LOG_ERR(Msg, ...) Xen::GetLogger().Log(Xen::Logger::Severity::Error, Msg, ##__VA_ARGS__)
#define LOG_CRIT(Msg, ...) Xen::GetLogger().Log(Xen::Logger::Severity::Critical, Msg, ##__VA_ARGS__)

#ifndef NDEBUG
    #define LOG_DBG(Msg, ...) Xen::GetLogger().Log(Xen::Logger::Severity::Debug, Msg, ##__VA_ARGS__)
#else
    #define LOG_DBG(Msg, ...)
#endif
