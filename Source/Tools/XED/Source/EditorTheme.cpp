//
// Created by Jake Rieger on 9/28/2026.
//

#include "EditorTheme.hpp"

namespace Xen {
    std::optional<EditorTheme> ThemeSerializer::LoadFromString(const std::string& JsonStr) {
        Json Root = Json::parse(JsonStr);
        if (!Root.is_object()) return None;

        const auto VersionIt = Root.find("version");
        if (VersionIt == Root.end() || !VersionIt->is_number()) return None;
        const u32 Version = VersionIt->get<u32>();
        if (Version != XED_THEME_FORMAT_VERSION) return None;

        const auto NameIt = Root.find("name");
        if (NameIt == Root.end() || !NameIt->is_string()) return None;
        const std::string Name = NameIt->get<std::string>();
        if (Name.empty()) return None;

        auto ParseColor = [](const std::string& ColorStr) -> Color { return Color {ColorStr}; };

        const auto ColorsIt = Root.find("colors");
        if (ColorsIt == Root.end() || !ColorsIt->is_object()) return None;

        const auto WindowBackgroundIt = ColorsIt->find("windowBackground");
        if (WindowBackgroundIt == ColorsIt->end() || !WindowBackgroundIt->is_string()) return None;
        Color WindowBackground = ParseColor(WindowBackgroundIt->get<std::string>());

        const auto PanelBackgroundIt = ColorsIt->find("panelBackground");
        if (PanelBackgroundIt == ColorsIt->end() || !PanelBackgroundIt->is_string()) return None;
        Color PanelBackground = ParseColor(PanelBackgroundIt->get<std::string>());

        const auto BorderIt = ColorsIt->find("border");
        if (BorderIt == ColorsIt->end() || !BorderIt->is_string()) return None;
        Color Border = ParseColor(BorderIt->get<std::string>());

        const auto TabActiveIt = ColorsIt->find("tabActive");
        if (TabActiveIt == ColorsIt->end() || !TabActiveIt->is_string()) return None;
        Color TabActive = ParseColor(TabActiveIt->get<std::string>());

        const auto TabInactiveIt = ColorsIt->find("tabInactive");
        if (TabInactiveIt == ColorsIt->end() || !TabInactiveIt->is_string()) return None;
        Color TabInactive = ParseColor(TabInactiveIt->get<std::string>());

        const auto ButtonPrimaryIt = ColorsIt->find("buttonPrimary");
        if (ButtonPrimaryIt == ColorsIt->end() || !ButtonPrimaryIt->is_string()) return None;
        Color ButtonPrimary = ParseColor(ButtonPrimaryIt->get<std::string>());

        const auto ButtonSecondaryIt = ColorsIt->find("buttonSecondary");
        if (ButtonSecondaryIt == ColorsIt->end() || !ButtonSecondaryIt->is_string()) return None;
        Color ButtonSecondary = ParseColor(ButtonSecondaryIt->get<std::string>());

        const auto InputIt = ColorsIt->find("input");
        if (InputIt == ColorsIt->end() || !InputIt->is_string()) return None;
        Color Input = ParseColor(InputIt->get<std::string>());

        const auto TextPrimaryIt = ColorsIt->find("textPrimary");
        if (TextPrimaryIt == ColorsIt->end() || !TextPrimaryIt->is_string()) return None;
        Color TextPrimary = ParseColor(TextPrimaryIt->get<std::string>());

        const auto TextSecondaryIt = ColorsIt->find("textSecondary");
        if (TextSecondaryIt == ColorsIt->end() || !TextSecondaryIt->is_string()) return None;
        Color TextSecondary = ParseColor(TextSecondaryIt->get<std::string>());

        const auto TextDisabledIt = ColorsIt->find("textDisabled");
        if (TextDisabledIt == ColorsIt->end() || !TextDisabledIt->is_string()) return None;
        Color TextDisabled = ParseColor(TextDisabledIt->get<std::string>());

        const auto WindowRoundingIt = Root.find("windowRounding");
        if (WindowRoundingIt == Root.end() || !WindowRoundingIt->is_number()) return None;
        const f32 WindowRounding = WindowRoundingIt->get<f32>();

        const auto FrameRoundingIt = Root.find("frameRounding");
        if (FrameRoundingIt == Root.end() || !FrameRoundingIt->is_number()) return None;
        const f32 FrameRounding = FrameRoundingIt->get<f32>();

        const auto TabRoundingIt = Root.find("tabRounding");
        if (TabRoundingIt == Root.end() || !TabRoundingIt->is_number()) return None;
        const f32 TabRounding = TabRoundingIt->get<f32>();

        const auto WindowBorderSizeIt = Root.find("windowBorderSize");
        if (WindowBorderSizeIt == Root.end() || !WindowBorderSizeIt->is_number()) return None;
        const f32 WindowBorderSize = WindowBorderSizeIt->get<f32>();

        const auto FrameBorderSizeIt = Root.find("frameBorderSize");
        if (FrameBorderSizeIt == Root.end() || !FrameBorderSizeIt->is_number()) return None;
        const f32 FrameBorderSize = FrameBorderSizeIt->get<f32>();

        return EditorTheme {
          .Colors =
            {
              .WindowBackground = WindowBackground,
              .PanelBackground  = PanelBackground,
              .Border           = Border,
              .TabActive        = TabActive,
              .TabInactive      = TabInactive,
              .ButtonPrimary    = ButtonPrimary,
              .ButtonSecondary  = ButtonSecondary,
              .Input            = Input,
              .TextPrimary      = TextPrimary,
              .TextSecondary    = TextSecondary,
              .TextDisabled     = TextDisabled,
            },
          .WindowRounding   = WindowRounding,
          .FrameRounding    = FrameRounding,
          .TabRounding      = TabRounding,
          .WindowBorderSize = WindowBorderSize,
          .FrameBorderSize  = FrameBorderSize,
        };
    }

    std::optional<EditorTheme> ThemeSerializer::LoadFromFile(const fs::path& ThemeFile) {
        if (!exists(ThemeFile)) return {};

        const std::ifstream F(ThemeFile);
        if (!F.is_open()) return {};

        std::ostringstream J;
        J << F.rdbuf();

        return LoadFromString(J.str());
    }
}  // namespace Xen