//
// Created by Jake Rieger on 10/12/2026.
//

#include "GenerateAssetIDModal.hpp"

#include <XenPAK/AssetID.hpp>
#include <XenPAK/Canonicalize.hpp>

#include <cstring>
#include <string>

namespace Xen {
    void GenerateAssetIDModal::DrawBody() {
        ImGui::InputText("Asset Path", _AssetPath.data(), _AssetPath.size());
        ImGui::InputText("Generated ID", _GeneratedID.data(), _GeneratedID.size());

        if (std::strlen(_AssetPath.data()) > 0) {
            const AssetID ID = PAK::HashPath(PAK::Canonicalize(_AssetPath.data()));
            const auto IDStr = std::to_string(ID.Value);
            std::snprintf(_GeneratedID.data(), _GeneratedID.size(), "%s", IDStr.c_str());
        }

        if (ImGui::Button("Close", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
    }
}  // namespace Xen
