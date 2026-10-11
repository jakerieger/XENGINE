//
// Created by Jake Rieger on 10/12/2026.
//

#include "AddComponentModal.hpp"
#include "../Editor.hpp"

namespace Xen {
    void AddComponentModal::DrawBody() {
        ImGui::Text("Select which component to add");
        ImGui::Separator();

        if (ImGui::BeginListBox("##Components", ImVec2(260.f, 220.f))) {
            for (const auto& Name : Editor::GetAddableComponentNames()) {
                if (ImGui::Selectable(Name.c_str(), _Choice == Name)) { _Choice = Name; }
            }
            ImGui::EndListBox();
        }

        ImGui::BeginDisabled(_Choice.empty());
        if (ImGui::Button("Add", ImVec2(120, 0))) {
            _Owner.AddComponentToSelected(_Choice);
            _Choice.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            _Choice.clear();
            ImGui::CloseCurrentPopup();
        }
    }
}  // namespace Xen
