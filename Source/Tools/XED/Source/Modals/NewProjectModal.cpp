//
// Created by Jake Rieger on 10/12/2026.
//

#include "NewProjectModal.hpp"
#include "../Editor.hpp"

#include <format>

namespace Xen {
    void NewProjectModal::Reset() {
        _ProjectName.fill(0);
        _ProjectDir.fill(0);
    }

    void NewProjectModal::DrawBody() {
        const std::string ProjectNameStr = _ProjectName.data();
        const std::string ProjectDirStr  = _ProjectDir.data();

        ImGui::InputText("Name", _ProjectName.data(), _ProjectName.size());

        ImGui::BeginDisabled(true);
        ImGui::InputText("##Location", _ProjectDir.data(), _ProjectDir.size());
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(ProjectNameStr.empty());
        if (ImGui::Button("...")) {
            const auto ProjectLocationResult =
              FileDialogs::OpenFolderDialog(_Owner._Window->GetHandle(), L"Select project directory");
            if (ProjectLocationResult.has_value()) {
                const auto Path = *ProjectLocationResult / ProjectNameStr;
                strcpy_s(_ProjectDir.data(), _ProjectDir.size(), Path.string().c_str());
            }
        }
        ImGui::EndDisabled();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            Reset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(ProjectNameStr.empty() || ProjectDirStr.empty());
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            const HWND Window   = _Owner._Window->GetHandle();
            const auto Result   = Editor::CreateProject(ProjectNameStr, ProjectDirStr);
            bool ProjectCreated = false;

            if (Result == Editor::CreateProjectResult::Success) {
                ProjectCreated = true;
            } else if (Result == Editor::CreateProjectResult::AlreadyExists) {
                const auto MsgResult = ::MessageBoxA(Window,
                                                     "The selected project directory already exists. Do you want "
                                                     "to overwrite it and create a new project anyways?",
                                                     "XED - Create project",
                                                     MB_YESNO | MB_ICONWARNING);
                if (MsgResult == IDYES) {
                    if (!fs::remove_all(ProjectDirStr)) {
                        ::MessageBoxA(Window,
                                      "Failed to remove existing directory.",
                                      "XED - Create project",
                                      MB_OK | MB_ICONERROR);
                    } else {
                        const auto TryAgainResult = Editor::CreateProject(ProjectNameStr, ProjectDirStr);
                        if (TryAgainResult != Editor::CreateProjectResult::Success) {
                            ::MessageBoxA(Window,
                                          "Failed to create new project.",
                                          "XED - Create project",
                                          MB_OK | MB_ICONERROR);
                        } else {
                            ProjectCreated = true;
                        }
                    }
                }
            } else if (Result == Editor::CreateProjectResult::Failed) {
                ::MessageBoxA(Window, "Failed to create new project.", "XED - Create project", MB_OK | MB_ICONERROR);
            }

            if (ProjectCreated) {
                const auto MsgFmt = std::format("Successfully created new project '{}'. Open it now?", ProjectNameStr);
                const auto MsgResult =
                  ::MessageBoxA(Window, MsgFmt.c_str(), "XED - Create project", MB_YESNO | MB_ICONQUESTION);
                if (MsgResult == IDYES) {
                    const auto PrxjPath = (fs::path(ProjectDirStr) / fs::path(ProjectNameStr)).replace_extension(".prxj");
                    _Owner.LoadProject(PrxjPath);
                }
            }

            Reset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SetItemDefaultFocus();
    }
}  // namespace Xen
