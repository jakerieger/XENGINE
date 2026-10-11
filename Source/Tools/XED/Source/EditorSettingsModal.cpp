//
// Created by Jake Rieger on 10/2/2026.
//

#include "EditorSettingsModal.hpp"
#include "UI.hpp"
#include "Editor.hpp"

namespace Xen {
    void EditorSettingsModal::Draw(Editor* Owner) {
        if (!Owner) return;

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
            if (_Settings.Save()) {
                ::MessageBoxA(nullptr, "Saved editor settings", "XED", MB_OK | MB_ICONINFORMATION);

                // Apply theme
                Owner->LoadTheme(_Settings.UITheme);
            } else {
                ::MessageBoxA(nullptr, "Failed to save editor settings", "XED", MB_OK | MB_ICONERROR);
            }
            ImGui::CloseCurrentPopup();
        } else if (Cancel) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    void EditorSettingsModal::DrawEditor() const {
        static char CurrentProject[MAX_PATH] {};
        strcpy_s(CurrentProject, MAX_PATH, _Settings.StartupProject.string().c_str());

        static const char* StartupMode[2] = {"Maximized", "Restore"};
        static int SelectedStartupMode    = 0;

        // static char UITheme[MAX_PATH] {};
        // strcpy_s(UITheme, MAX_PATH, _Settings.UITheme.c_str());

        std::vector<std::string> Themes;
        for (const auto& it : fs::recursive_directory_iterator("Config/Themes")) {
            if (fs::is_regular_file(it) && it.path().extension() == ".json") {
                Themes.push_back(it.path().filename().stem().string());
            }
        }
        static int SelectedTheme = 0;

        ImGui::SeparatorText("Editor");
        if (UI::Controls::BeginSettingsTable("##editor")) {
            UI::Controls::SettingsRow("Current Project");
            ImGui::InputText("##CurrentProject", CurrentProject, MAX_PATH);

            UI::Controls::SettingsRow("Startup Mode");
            ImGui::Combo("##StartupMode", &SelectedStartupMode, StartupMode, 2);

            UI::Controls::SettingsRow("UI Theme");
            if (ImGui::BeginCombo("##UITheme", Themes[SelectedTheme].c_str())) {
                for (int i = 0; i < Themes.size(); i++) {
                    const bool Selected = (SelectedTheme == i);
                    if (ImGui::Selectable(Themes[i].c_str(), Selected)) { SelectedTheme = i; }
                    if (Selected) { ImGui::SetItemDefaultFocus(); }
                }

                ImGui::EndCombo();
            }

            UI::Controls::EndSettingsTable();
        }

        _Settings.StartupProject = CurrentProject;
        _Settings.StartupMode    = SelectedStartupMode == 0 ? EditorStartupMode::Maximized : EditorStartupMode::Restore;
        _Settings.UITheme        = Themes[SelectedTheme];
    }

    void EditorSettingsModal::DrawExternalTools() const {
        static const char* CodeEditorOptions[] = {"None", "Visual Studio Code", "VS Codium", "CLion"};
        static int SelectedCodeEditor          = 0;

        ImGui::SeparatorText("External Tools");

        if (UI::Controls::BeginSettingsTable("##externaltools")) {
            UI::Controls::SettingsRow("Code Editor");
            if (ImGui::BeginCombo("##Code Editor", CodeEditorOptions[SelectedCodeEditor])) {
                for (int i = 0; i < IM_ARRAYSIZE(CodeEditorOptions); i++) {
                    const bool Selected = (SelectedCodeEditor == i);

                    const auto Editor     = CAST<CodeEditor>(i);
                    const auto EditorPath = GetCodeEditorPath(Editor);
                    ImGui::BeginDisabled(i != 0 && !exists(*EditorPath));
                    if (ImGui::Selectable(CodeEditorOptions[i], Selected)) { SelectedCodeEditor = i; }
                    ImGui::EndDisabled();

                    if (Selected) { ImGui::SetItemDefaultFocus(); }
                }

                ImGui::EndCombo();
            }

            UI::Controls::EndSettingsTable();
        }

        if (SelectedCodeEditor == 0) _Settings.ExternalTools.CodeEditor = CodeEditor::None;
        else if (SelectedCodeEditor == 1) _Settings.ExternalTools.CodeEditor = CodeEditor::VSCode;
        else if (SelectedCodeEditor == 2) _Settings.ExternalTools.CodeEditor = CodeEditor::VSCodium;
        else if (SelectedCodeEditor == 3) _Settings.ExternalTools.CodeEditor = CodeEditor::CLion;
    }
}  // namespace Xen