//
// Created by Jake Rieger on 10/2/2026.
//

#pragma once

#include "EditorSettings.hpp"

namespace Xen {
    class EditorSettingsModal {
    public:
        explicit EditorSettingsModal(EditorSettings& Settings) : _Settings(Settings) {};

        void Open() { _RequestOpen = true; }

        void Draw();

    private:
        enum Category {
            Category_Editor,
            Category_ExternalTools,
            NumCategories,
        };

        static constexpr const char* POPUP_ID                      = "Settings";
        static constexpr const char* CATEGORY_NAMES[NumCategories] = {"Editor", "External Tools"};

        void DrawEditor();
        void DrawExternalTools();

        EditorSettings& _Settings;
        Category _Category {Category_Editor};
        bool _RequestOpen {false};
    };
}  // namespace Xen