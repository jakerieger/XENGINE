//
// Created by Jake Rieger on 10/12/2026.
//

#include "NewFolderModal.hpp"

#include <cstdio>

namespace Xen {
    void NewFolderModal::Open(const fs::path& Parent, Callback OnCreated) {
        _Parent    = Parent;
        _OnCreated = std::move(OnCreated);
        _Error.clear();

        std::string Name = "New Folder";
        for (int N = 2; fs::exists(_Parent / Name); ++N)
            Name = "New Folder " + std::to_string(N);
        std::snprintf(_Buffer, sizeof(_Buffer), "%s", Name.c_str());

        RequestOpen();
    }

    void NewFolderModal::DrawBody() {
        ImGui::Text("New folder in '%s'", _Parent.filename().string().c_str());
        ImGui::Spacing();

        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(260.0f);
        const bool Submitted = ImGui::InputText("##foldername",
                                                _Buffer,
                                                sizeof(_Buffer),
                                                ImGuiInputTextFlags_EnterReturnsTrue |
                                                  ImGuiInputTextFlags_AutoSelectAll);

        // Re-checked every frame so the message follows what's typed.
        const fs::path Destination = _Parent / _Buffer;
        std::string Problem        = ValidateFileName(_Buffer);
        if (Problem.empty() && fs::exists(Destination)) Problem = "'" + std::string(_Buffer) + "' already exists here.";

        if (!Problem.empty()) ErrorText(Problem);
        if (!_Error.empty()) ErrorText(_Error);
        ImGui::Spacing();

        ImGui::BeginDisabled(!Problem.empty());
        const bool Confirm = ImGui::Button("Create", ImVec2(120, 0)) || (Submitted && Problem.empty());
        ImGui::EndDisabled();
        ImGui::SameLine();
        const bool Cancel = ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false);

        if (Confirm) {
            std::error_code Error;
            fs::create_directory(Destination, Error);
            if (Error) {
                _Error = "Couldn't create the folder: " + Error.message();
            } else {
                if (_OnCreated) _OnCreated(Destination);
                ImGui::CloseCurrentPopup();
            }
        } else if (Cancel) {
            ImGui::CloseCurrentPopup();
        }
    }
}  // namespace Xen
