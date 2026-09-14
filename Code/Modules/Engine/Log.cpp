//
// Created by Jake Rieger on 9/13/2026.
//

#include "Log.hpp"

#include "DateTime.hpp"
#include "Exception.hpp"

#include <filesystem>

namespace Xen {
    namespace fs = std::filesystem;

    Logger& GetLogger() {
        static Logger Instance;
        return Instance;
    }

    Logger::Logger() {
        _LogFileName = GetLogFileName();
        _LogStream.open(_LogFileName, std::ios::app);

        const auto Header = std::format("-- Log opened at {} --\n", DateTime::Now().LocalUTCString());
        _LogStream << Header;
    }

    Logger::~Logger() {
        if (_LogStream.is_open()) {
            std::printf("Log saved to '%s'\n", _LogFileName.c_str());
            _LogStream.close();
        }
    }

    void Logger::Log(const Severity Sev, const char* Msg) {
        const auto SeverityString = GetSeverityString(Sev);
        const auto TimeStamp      = GetTimeStamp();
        const auto LogEntry       = std::format("[{}] | {} | {}\n", TimeStamp, SeverityString, Msg);

        _LogStream << LogEntry;
        _LogStream.flush();

#ifndef NDEBUG

        if (const u8 SevUnsigned = CAST<u8>(Sev);
            SevUnsigned > CAST<u8>(Severity::Info) && SevUnsigned < CAST<u8>(Severity::Debug)) {
            std::fprintf(stderr, "%s", LogEntry.c_str());
        } else {
            std::printf("%s", LogEntry.c_str());
        }
#endif

        {
            std::lock_guard Lock(_BufferMutex);
            _Entries[_CurrentEntry] = {
              .Message   = Msg,
              .TimeStamp = TimeStamp,
              .Severity  = Sev,
            };
            _CurrentEntry = (_CurrentEntry + 1) % LOGGER_MAX_ENTRIES;
            _TotalEntries = std::min<size_t>(_TotalEntries + 1, LOGGER_MAX_ENTRIES);
        }
    }

    void Logger::Clear() {
        std::lock_guard Lock(_BufferMutex);
        _TotalEntries = 0;
        _CurrentEntry = 0;
    }

    std::string Logger::GetTimeStamp() {
        return DateTime::Now().TimeString();
    }

    std::string Logger::GetSeverityString(const Severity Sev) {
        switch (Sev) {
            case Severity::Info:
                return "INFO";
            case Severity::Warning:
                return "WARNING";
            case Severity::Error:
                return "ERROR";
            case Severity::Critical:
                return "CRITICAL";
            case Severity::Debug:
            default:
                return "DEBUG";
        }
    }

    std::string Logger::GetLogFileName() {
        static fs::path LogDirectory = fs::current_path() / "Logs";
        if (!exists(LogDirectory)) {
            if (const bool Result = fs::create_directories(LogDirectory); !Result) {
                _ThrowEngineException(LogException,
                                      std::format("failed to create logs directory at: {}", LogDirectory.string()));
            }
        }

        const auto UnixMillis = DateTime::Now().UnixEpochString();
        const auto FileName   = "Session_" + UnixMillis + ".log";
        const auto LogPath    = (LogDirectory / FileName);

        return LogPath.string();
    }
}  // namespace Xen