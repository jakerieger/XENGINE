//
// Created by Jake Rieger on 9/13/2026.
//

#pragma once

#include "XenCommon.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace Xen {
    class DateTime {
        DateTime(const DateTime&)            = delete;
        DateTime(DateTime&&)                 = delete;
        DateTime& operator=(const DateTime&) = delete;
        DateTime& operator=(DateTime&&)      = delete;

    public:
        using Timepoint = std::chrono::time_point<std::chrono::system_clock>;

        explicit DateTime(const Timepoint Time) : _Time(Time) {}

        static DateTime Now() { return {}; }

        /// @brief Returns a string in the following format:
        /// `YYYY-MM-DD HH:MM:SS AM/PM`
        _NoDiscard std::string UTCString() const {
            const auto T = std::chrono::system_clock::to_time_t(_Time);
            std::tm Tm {};
            gmtime_s(&Tm, &T);

            return FormatDateTimeString(Tm);
        }

        /// @brief Returns a string in the following format:
        /// `YYYY-MM-DD HH:MM:SS AM/PM`
        _NoDiscard std::string LocalUTCString() const {
            const auto T = std::chrono::system_clock::to_time_t(_Time);
            std::tm Tm {};
            localtime_s(&Tm, &T);

            return FormatDateTimeString(Tm);
        }

        /// @brief Returns a string in the following format:
        /// `YYYY-MM-DD`
        _NoDiscard std::string DateString() const {
            const auto T = std::chrono::system_clock::to_time_t(_Time);
            std::tm Tm {};
            localtime_s(&Tm, &T);

            std::ostringstream Oss;
            Oss << std::put_time(&Tm, "%Y-%m-%d");

            return Oss.str();
        }

        /// @brief Returns a string in the following format:
        /// `HH:MM:SS AM/PM`
        _NoDiscard std::string TimeString() const {
            const auto T = std::chrono::system_clock::to_time_t(_Time);
            std::tm Tm {};
            localtime_s(&Tm, &T);

            return FormatTimeString(Tm);
        }

        _NoDiscard i64 UnixEpoch() const {
            const auto Duration   = _Time.time_since_epoch();
            const auto UnixMillis = std::chrono::duration_cast<std::chrono::milliseconds>(Duration).count();

            return UnixMillis;
        }

        _NoDiscard std::string UnixEpochString() const { return std::to_string(UnixEpoch()); }

    private:
        Timepoint _Time;

        DateTime() : _Time(std::chrono::system_clock::now()) {}

        static std::string FormatDateTimeString(const std::tm& Tm) {
            std::ostringstream Oss;
            Oss << std::put_time(&Tm, "%Y-%m-%d");
            Oss << " ";
            Oss << FormatTimeString(Tm);

            return Oss.str();
        }

        static std::string FormatTimeString(const std::tm& Tm) {
            std::ostringstream Oss;

            i32 Hour        = Tm.tm_hour;
            const bool IsPM = Tm.tm_hour >= 12;
            Hour            = Hour % 12;
            if (Hour == 0) Hour = 12;

            Oss << std::setfill('0') << std::setw(2) << Hour << ":" << std::setw(2) << Tm.tm_min << ":" << std::setw(2)
                << Tm.tm_sec << (IsPM ? " PM" : " AM");

            return Oss.str();
        }
    };
}  // namespace Xen