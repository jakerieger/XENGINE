//
// Created by Jake Rieger on 10/12/2026.
//

#include "NewSceneModal.hpp"
#include "../Editor.hpp"

#include <algorithm>

namespace Xen {
    void NewSceneModal::DrawBody() {
        const std::string SceneName = _SceneName.data();
        ImGui::InputText("Scene Name", _SceneName.data(), _SceneName.size());

        ImGui::BeginDisabled(SceneName.empty());
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            const auto ScenesDir = _Owner._CurrentProject.ContentDirectory / "scenes";
            if (!exists(ScenesDir)) { fs::create_directory(ScenesDir); }

            std::string SceneFileName = SceneName;
            std::ranges::transform(SceneFileName, SceneFileName.begin(), ::tolower);
            const auto SceneFilePath = (ScenesDir / SceneFileName).replace_extension(".xscene");

            _Owner.CreateScene(SceneName, SceneFilePath);

            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();

        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
    }
}  // namespace Xen
