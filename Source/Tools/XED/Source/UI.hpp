//
// Created by Jake Rieger on 10/3/2026.
//

#pragma once

#include <imgui.h>
#include <imgui_internal.h>

namespace Xen::UI {
    inline void CenterNextWindow() {
        const ImVec2 Center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(Center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    }
}  // namespace Xen::UI