//
// Created by Jake Rieger on 9/28/2026.
//

#pragma once

#include <Common/XenCommon.hpp>
#include <nlohmann/json.hpp>

namespace Xen {
    constexpr u32 XED_THEME_FORMAT_VERSION = 1;

    using Json = nlohmann::ordered_json;

    struct Theme {};

    class ThemeSerializer {
        ThemeSerializer() = delete;

    public:
        static std::optional<Theme> LoadFromString(const std::string& JsonStr);
        static std::optional<Theme> LoadFromFile(const std::filesystem::path& ThemeFile);
        // TODO: Save variants
    };
}  // namespace Xen