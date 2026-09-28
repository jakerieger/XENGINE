//
// Created by Jake Rieger on 9/28/2026.
//

#pragma once

#include <ini.h>
#include <optional>

namespace Xen::INI {
    using Config = mINI::INIStructure;

    inline std::optional<Config> ReadFromFile(const std::filesystem::path& IniFile) {
        const mINI::INIFile File(IniFile);
        Config OutConfig;

        if (!File.read(OutConfig)) {
            LOG_WARN("failed to read ini file: '%s'", IniFile.string().c_str());
            return {};
        }

        return OutConfig;
    }

    inline u32 GetU32(const std::string& Val) {
        return CAST<u32>(std::stoi(Val));
    }

    inline f32 GetF32(const std::string& Val) {
        return std::stof(Val);
    }

    /// @brief Splits a list string value (',' delim) and returns it as a string vector.
    ///
    /// Example value: red,green,blue (no spaces)
    inline std::vector<std::string> GetList(const std::string& Val) {
        auto Split = [](const std::string& value) -> std::vector<std::string> {
            std::vector<std::string> Result;
            std::stringstream Stream(value);
            std::string Token;
            while (std::getline(Stream, Token, ',')) {
                Result.push_back(Token);
            }
            return Result;
        };

        return Split(Val);
    }
}  // namespace Xen::INI