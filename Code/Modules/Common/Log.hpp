//
// Created by Jake Rieger on 9/13/2026.
//

#pragma once

#include "XenCommon.hpp"
#include "Exception.hpp"

#include <array>
#include <fstream>
#include <mutex>

namespace Xen {
    _DefineEngineException(LogException);

    class Logger;
    Logger& GetLogger();

    class Logger {
        friend Logger& GetLogger();
        Logger(const Logger&)            = delete;
        Logger(Logger&&)                 = delete;
        Logger& operator=(const Logger&) = delete;
        Logger& operator=(Logger&&)      = delete;

    public:
        static constexpr size_t LOGGER_MAX_ENTRIES = _Kb(4);

        enum class Severity : u8 {
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
            char Buffer[_Kb(1)];
            std::snprintf(Buffer, sizeof(Buffer), Fmt, FmtArgs...);
            Log(Sev, Buffer);
        }

        std::mutex& GetBufferMutex() { return _BufferMutex; }
        size_t GetTotalEntries() const { return _TotalEntries; }
        size_t GetCurrentEntryIndex() const { return _CurrentEntry; }
        std::array<Entry, LOGGER_MAX_ENTRIES> const& GetEntries() const { return _Entries; }

    private:
        std::ofstream _LogStream;
        std::string _LogFileName;
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

#define _LogInfo(Msg, ...) Xen::GetLogger().Log(Xen::Logger::Severity::Info, Msg, ##__VA_ARGS__)
#define _LogWarning(Msg, ...) Xen::GetLogger().Log(Xen::Logger::Severity::Warning, Msg, ##__VA_ARGS__)
#define _LogError(Msg, ...) Xen::GetLogger().Log(Xen::Logger::Severity::Error, Msg, ##__VA_ARGS__)
#define _LogCritical(Msg, ...) Xen::GetLogger().Log(Xen::Logger::Severity::Critical, Msg, ##__VA_ARGS__)

#ifndef NDEBUG
    #define _LogDebug(Msg, ...) Xen::GetLogger().Log(Xen::Logger::Severity::Debug, Msg, ##__VA_ARGS__)
#else
    #define _LogDebug(Msg, ...)
#endif
