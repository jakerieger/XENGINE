//
// Created by Jake Rieger on 10/12/2026.
//

#include "SaveAsPrefabModal.hpp"
#include "../Editor.hpp"

#include <Common/Log.hpp>
#include <Xen/SceneSerializer.hpp>

#include <cstdio>

namespace Xen {
    void SaveAsPrefabModal::Open(const ActorHandle Source, const std::string& DefaultName) {
        _Source = Source;
        _Error.clear();
        _Name.fill('\0');
        std::snprintf(_Name.data(), _Name.size(), "%s", DefaultName.c_str());
        RequestOpen();
    }

    void SaveAsPrefabModal::DrawBody() {
        Scene* Active       = _Owner.ActiveScene();
        const Actor* Source = Active ? Active->Get(_Source) : nullptr;
        if (!Source) {
            // Gone since the menu was opened (deleted, scene switched).
            ImGui::TextDisabled("The actor no longer exists.");
            if (ImGui::Button("Close", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
            return;
        }

        const fs::path PrefabsDir = _Owner._CurrentProject.ContentDirectory / "Prefabs";
        const std::string Name    = _Name.data();

        ImGui::Text("Save '%s' and its children as a prefab", Source->GetName().c_str());
        ImGui::Spacing();

        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(260.0f);
        const bool Submitted =
          ImGui::InputText("##prefabname",
                           _Name.data(),
                           _Name.size(),
                           ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        ImGui::SameLine();
        ImGui::TextDisabled(".xprefab");

        // Same file-name rules as the content browser's rename.
        const std::string Problem = ValidateFileName(Name);
        const fs::path Target     = PrefabsDir / (Name + ".xprefab");
        const bool Exists         = Problem.empty() && fs::exists(Target);

        ImGui::TextDisabled("Saved to Content/Prefabs/");
        if (!Problem.empty()) ErrorText(Problem);
        if (Exists) WarningText("A prefab with this name exists and will be replaced.");
        if (!_Error.empty()) ErrorText(_Error);
        ImGui::Spacing();

        ImGui::BeginDisabled(!Problem.empty());
        const bool Confirm =
          ImGui::Button(Exists ? "Replace" : "Save", ImVec2(120, 0)) || (Submitted && Problem.empty());
        ImGui::EndDisabled();
        ImGui::SameLine();
        const bool Cancel = ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false);

        if (Confirm) {
            try {
                SceneSerializer::SavePrefabToFile(*Active, _Source, Target);
                LOG_INFO("Saved prefab '%s'", Target.string().c_str());

                // Both the asset index (inspector slots, drop targets) and the browser's folder listing
                _Owner._AssetIndex.Rescan(_Owner._CurrentProject.ContentDirectory);
                _Owner._ContentBrowser.NotifyContentChanged();
                ImGui::CloseCurrentPopup();
            } catch (const std::exception& Error) { _Error = std::string("Couldn't save: ") + Error.what(); }
        } else if (Cancel) {
            ImGui::CloseCurrentPopup();
        }
    }
}  // namespace Xen
