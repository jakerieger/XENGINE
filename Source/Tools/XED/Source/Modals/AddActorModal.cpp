//
// Created by Jake Rieger on 10/13/2026.
//

#include "AddActorModal.hpp"
#include "../Editor.hpp"

#include <cstdio>

namespace Xen {
    void AddActorModal::Open() {
        _NameEdited = false;
        SelectPreset(ActorPreset::Empty);
        RequestOpen();
    }

    void AddActorModal::SelectPreset(const ActorPreset Preset) {
        _Preset = Preset;
        if (!_NameEdited) {
            std::snprintf(_Name.data(), _Name.size(), "%s", GetActorPresetInfo(Preset).DefaultName);
        }
    }

    void AddActorModal::DrawBody() {
        ImGui::Text("What do you want to create?");
        ImGui::Separator();

        if (ImGui::BeginListBox("##presets", ImVec2(300.0f, 190.0f))) {
            for (const ActorPresetInfo& Info : GetActorPresets()) {
                if (ImGui::Selectable(Info.Label, _Preset == Info.Preset, ImGuiSelectableFlags_AllowDoubleClick)) {
                    SelectPreset(Info.Preset);
                    // Double-click creates it right away.
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        _Owner.CreateActor(_Preset, _Name.data());
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            ImGui::EndListBox();
        }
        ImGui::TextDisabled("%s", GetActorPresetInfo(_Preset).Description);
        ImGui::Spacing();

        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(300.0f);
        const bool Submitted = ImGui::InputText("##name",
                                                _Name.data(),
                                                _Name.size(),
                                                ImGuiInputTextFlags_EnterReturnsTrue |
                                                  ImGuiInputTextFlags_AutoSelectAll);
        if (ImGui::IsItemEdited()) _NameEdited = true;
        ImGui::SameLine();
        ImGui::TextDisabled("Name");
        ImGui::Spacing();

        const bool HasName = _Name[0] != '\0';
        ImGui::BeginDisabled(!HasName);
        const bool Create = ImGui::Button("Create", ImVec2(120, 0)) || (Submitted && HasName);
        ImGui::EndDisabled();
        ImGui::SameLine();
        const bool Cancel = ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false);

        if (Create) {
            _Owner.CreateActor(_Preset, _Name.data());
            ImGui::CloseCurrentPopup();
        } else if (Cancel) {
            ImGui::CloseCurrentPopup();
        }
    }
}  // namespace Xen
