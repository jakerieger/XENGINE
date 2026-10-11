//
// Created by Jake Rieger on 10/12/2026.
//

#include "NewComponentClassModal.hpp"
#include "../Editor.hpp"

namespace Xen {
    void NewComponentClassModal::DrawBody() {
        const std::string ClassNameStr = _ClassName.data();
        ImGui::InputText("Name", _ClassName.data(), _ClassName.size());

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            _ClassName.fill(0);
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        ImGui::BeginDisabled(ClassNameStr.empty());
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            _Owner.CreateComponentClass(ClassNameStr);
            _ClassName.fill(0);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
    }
}  // namespace Xen
