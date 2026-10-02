//
// Created by Jake Rieger on 9/28/2026.
//

#pragma once

#include <Common/XenCommon.hpp>
#include <Common/Platform.hpp>
#include <Xen/Color.hpp>
#include <nlohmann/json.hpp>
#include <imgui.h>

namespace Xen {
    constexpr u32 XED_THEME_FORMAT_VERSION = 1;

    using Json = nlohmann::ordered_json;

    template<>
    inline ImVec4 Color::To() const {
        return ImVec4 {_R, _G, _B, _A};
    }

    struct EditorTheme {
        struct {
            Color WindowBackground {};
            Color PanelBackground {};
            Color Border {};
            Color TabActive {};
            Color TabInactive {};
            Color ButtonPrimary {};
            Color ButtonSecondary {};
            Color Input {};
            Color TextPrimary {};
            Color TextSecondary {};
            Color TextDisabled {};
        } Colors;

        f32 WindowRounding {0.0f};
        f32 FrameRounding {0.0f};
        f32 TabRounding {0.0f};
        f32 WindowBorderSize {1.0f};
        f32 FrameBorderSize {1.0f};
    };

    class ThemeSerializer {
        ThemeSerializer() = delete;

    public:
        static std::optional<EditorTheme> LoadFromString(const std::string& JsonStr);
        static std::optional<EditorTheme> LoadFromFile(const fs::path& ThemeFile);
        // TODO: Save variants
    };
}  // namespace Xen