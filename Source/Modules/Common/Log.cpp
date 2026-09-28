//
// Created by Jake Rieger on 9/13/2026.
//

#include "XenCommon.hpp"

#include <filesystem>

namespace Xen {
    namespace fs               = std::filesystem;
    constexpr auto LogFileName = "Game.log";

    Logger& GetLogger() {
        static Logger Instance;
        return Instance;
    }

    Logger::Logger() {
        _LogStream.open(LogFileName, std::ios::out | std::ios::trunc);

        const auto Header = std::format("-- Log opened at {} --\n", DateTime::Now().LocalUTCString());
        _LogStream << Header;
    }

    Logger::~Logger() {
        if (_LogStream.is_open()) _LogStream.close();

        // Copy log to 'Logs' directory for backup. 'Game.log' gets overwritten the next time the game runs.
        const auto BackupFileName = GetLogFileName();
        fs::copy_file(LogFileName, BackupFileName);
        if (fs::exists(BackupFileName)) {
            std::printf("Log has been backed up to '%s'\n", BackupFileName.c_str());
        } else {
            std::fprintf(stderr, "Failed to backup current log to '%s'\n", BackupFileName.c_str());
        }
    }

    void Logger::Log(const Severity Sev, const char* Msg) {
        const auto SeverityString = GetSeverityString(Sev);
        const auto TimeStamp      = GetTimeStamp();
        const auto LogEntry       = std::format("[{}] | {} | {}\n", TimeStamp, SeverityString, Msg);

        // One lock over the file, the console and the ring buffer: AssetLoader's
        // worker threads log too, and unlocked writes to the same ofstream/
        // stdout interleave mid-line.
        std::lock_guard Lock(_BufferMutex);

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

        _Entries[_CurrentEntry] = {
          .Message   = Msg,
          .TimeStamp = TimeStamp,
          .Severity  = Sev,
        };
        _CurrentEntry = (_CurrentEntry + 1) % LOGGER_MAX_ENTRIES;
        _TotalEntries = std::min<size_t>(_TotalEntries + 1, LOGGER_MAX_ENTRIES);
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
                PANIC("failed to create logs directory at: %s", LogDirectory.string().c_str());
            }
        }

        const auto UnixMillis = DateTime::Now().UnixEpochString();
        const auto FileName   = "Session_" + UnixMillis + ".log";
        const auto LogPath    = (LogDirectory / FileName);

        return LogPath.string();
    }
}  // namespace Xen