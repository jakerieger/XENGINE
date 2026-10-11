//
// Created by Jake Rieger on 10/12/2026.
//

#include "RenameModal.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace Xen {
    namespace {
        std::string Lowercase(std::string S) {
            std::ranges::transform(S, S.begin(), [](const unsigned char C) { return CAST<char>(std::tolower(C)); });
            return S;
        }
    }  // namespace

    void RenameModal::Open(const fs::path& Target, const bool IsDirectory, Callback OnRenamed) {
        _Target      = Target;
        _IsDirectory = IsDirectory;
        _OnRenamed   = std::move(OnRenamed);
        _Error.clear();

        // Files are renamed without their extension.
        const std::string Stem = IsDirectory ? Target.filename().string() : Target.stem().string();
        std::snprintf(_Buffer, sizeof(_Buffer), "%s", Stem.c_str());
        RequestOpen();
    }

    void RenameModal::DrawBody() {
        const std::string Extension = _IsDirectory ? std::string() : _Target.extension().string();

        ImGui::Text("Rename '%s'", _Target.filename().string().c_str());
        ImGui::Spacing();

        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(260.0f);
        const bool Submitted = ImGui::InputText("##newname",
                                                _Buffer,
                                                sizeof(_Buffer),
                                                ImGuiInputTextFlags_EnterReturnsTrue |
                                                  ImGuiInputTextFlags_AutoSelectAll);
        if (!Extension.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", Extension.c_str());
        }

        // Re-checked every frame so the message follows what's typed.
        const std::string OldName  = _Target.filename().string();
        const std::string NewName  = std::string(_Buffer) + Extension;
        const fs::path Destination = _Target.parent_path() / NewName;
        std::string Problem        = ValidateFileName(_Buffer);

        // A change of case alone is fine (Windows treats the names as the same file).
        const bool OnlyCaseChanged = Lowercase(NewName) == Lowercase(OldName);
        if (Problem.empty() && NewName != OldName && !OnlyCaseChanged && fs::exists(Destination)) {
            Problem = "'" + NewName + "' already exists here.";
        }

        ImGui::TextDisabled("References to it in scenes and components will show as Missing.");
        if (!Problem.empty()) ErrorText(Problem);
        if (!_Error.empty()) ErrorText(_Error);
        ImGui::Spacing();

        const bool Unchanged = NewName == OldName;
        ImGui::BeginDisabled(!Problem.empty() || Unchanged);
        const bool Confirm = ImGui::Button("Rename", ImVec2(120, 0)) || (Submitted && Problem.empty() && !Unchanged);
        ImGui::EndDisabled();
        ImGui::SameLine();
        const bool Cancel = ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false);

        if (Confirm) {
            std::error_code Error;
            fs::rename(_Target, Destination, Error);
            if (Error) {
                _Error = "Couldn't rename: " + Error.message();
            } else {
                if (_OnRenamed) _OnRenamed(Destination);
                ImGui::CloseCurrentPopup();
            }
        } else if (Cancel) {
            ImGui::CloseCurrentPopup();
        }
    }
}  // namespace Xen
