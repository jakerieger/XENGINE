//
// Created by Jake Rieger on 10/2/2026.
//

#include "EditorSettingsModal.hpp"

#include <imgui.h>
#include <imgui_internal.h>

namespace Xen {
    void EditorSettingsModal::Draw() {
        if (_RequestOpen) {
            ImGui::OpenPopup(POPUP_ID);
            _RequestOpen = false;
        }

        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(720.0f, 460.0f), ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints(ImVec2(480.0f, 300.0f), ImVec2(FLT_MAX, FLT_MAX));

        if (!ImGui::BeginPopupModal(POPUP_ID,
                                    nullptr,
                                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar)) {
            return;
        }

        bool Cancel = ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !ImGui::IsAnyItemActive();
        bool Save   = false;

        const ImGuiStyle& Style = ImGui::GetStyle();
        const f32 FooterHeight  = Style.ItemSpacing.y + 1.0f + ImGui::GetFrameHeightWithSpacing();

        ImGui::BeginChild("##body", ImVec2(0.0f, -FooterHeight));
        {
            const f32 ListWidth = ImGui::GetFontSize() * 10.0f;
            if (ImGui::BeginListBox("##categories", ImVec2(ListWidth, -FLT_MIN))) {
                for (int i = 0; i < NumCategories; i++) {
                    const bool Selected = (_Category == i);
                    if (ImGui::Selectable(CATEGORY_NAMES[i], Selected)) { _Category = CAST<Category>(i); }
                    if (Selected) { ImGui::SetItemDefaultFocus(); }
                }

                ImGui::EndListBox();
            }

            ImGui::SameLine();

            ImGui::BeginChild("##page", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
            {
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
                switch (_Category) {
                    case Category_Editor:
                        DrawEditor();
                        break;
                    case Category_ExternalTools:
                        DrawExternalTools();
                        break;
                    default:
                        break;
                }
                ImGui::PopItemWidth();
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();

        ImGui::Dummy(ImVec2(0.0f, 1.0f));

        const f32 ButtonWidth  = ImGui::GetFontSize() * 6.0f;
        const f32 ButtonsTotal = ButtonWidth * 2.0f + Style.ItemSpacing.x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - ButtonsTotal);

        if (ImGui::Button("Save", ImVec2(ButtonWidth, 0.0f))) { Save = true; }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(ButtonWidth, 0.0f))) { Cancel = true; }

        if (Save) {
            if (_Settings.Write()) {
                ::MessageBoxA(nullptr, "Saved editor settings", "XED", MB_OK | MB_ICONINFORMATION);
            } else {
                ::MessageBoxA(nullptr, "Failed to save editor settings", "XED", MB_OK | MB_ICONERROR);
            }
            ImGui::CloseCurrentPopup();
        } else if (Cancel) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    void EditorSettingsModal::DrawEditor() {
        static char CurrentProject[MAX_PATH] {};
        strcpy_s(CurrentProject, MAX_PATH, _Settings.CurrentProject.string().c_str());

        static const char* StartupMode[2] = {"Maximized", "Restore"};
        static int SelectedStartupMode    = 0;

        static char UITheme[MAX_PATH] {};
        strcpy_s(UITheme, MAX_PATH, _Settings.UITheme.c_str());

        ImGui::SeparatorText("Editor");
        ImGui::InputText("Current Project", CurrentProject, MAX_PATH);
        ImGui::Combo("Startup Mode", &SelectedStartupMode, StartupMode, 2);
        ImGui::InputText("UI Theme", UITheme, MAX_PATH);

        _Settings.CurrentProject = CurrentProject;
        _Settings.StartupMode    = SelectedStartupMode == 0 ? EditorStartupMode::Maximized : EditorStartupMode::Restore;
        _Settings.UITheme        = UITheme;
    }

    void EditorSettingsModal::DrawExternalTools() {}
}  // namespace Xen