//
// Created by Jake Rieger on 10/3/2026.
//

#pragma once

#include <imgui.h>
#include <imgui_internal.h>

#include <Xen/Color.hpp>

namespace Xen::UI {
    inline void CenterNextWindow() {
        const ImVec2 Center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(Center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    }

    namespace Controls {
        inline bool DragFloatNColored(const char* Label,
                                      f32* V,
                                      const i32 Components,
                                      f32 Speed       = 0.1f,
                                      f32 Min         = -FLT_MAX,
                                      f32 Max         = FLT_MAX,
                                      const char* Fmt = "%.3f",
                                      f32 Power       = 1.0f) {
            const ImGuiWindow* Window = ImGui::GetCurrentWindow();
            if (Window->SkipItems) return false;

            const ImGuiContext& Ctx = *GImGui;
            bool ValueChanged       = false;

            ImGui::BeginGroup();
            {
                ImGui::PushID(Label);
                ImGui::PushMultiItemsWidths(Components, ImGui::CalcItemWidth());

                // Need color as unsigned int in ABGR format. Why this isn't consistent across ImGui? No clue.
                constexpr ColorChannelOrder ABGRFormat = Channel::A | Channel::B | Channel::G | Channel::R;

                const ImU32 R = Color("#eb3751").GetComponents(ABGRFormat);
                const ImU32 G = Color("#83cb10").GetComponents(ABGRFormat);
                const ImU32 B = Color("#2f85e6").GetComponents(ABGRFormat);

                for (i32 i = 0; i < Components; ++i) {
                    static const ImU32 Colors[] = {R, G, B, 0xBBFFFFFF};

                    ImGui::PushID(i);
                    ValueChanged |= ImGui::DragFloat("##v", &V[i], Speed, Min, Max, Fmt, 0);

                    const ImVec2 RectMin  = ImGui::GetItemRectMin();
                    const ImVec2 RectMax  = ImGui::GetItemRectMax();
                    const f32 Spacing     = Ctx.Style.FrameRounding;
                    const f32 HalfSpacing = Spacing * 0.5f;

                    Window->DrawList->AddLine({RectMin.x + Spacing, RectMax.y - HalfSpacing},
                                              {RectMax.x - Spacing, RectMax.y - HalfSpacing},
                                              Colors[i],
                                              i);

                    ImGui::SameLine(0, Ctx.Style.ItemInnerSpacing.x);
                    ImGui::PopID();
                    ImGui::PopItemWidth();
                }

                ImGui::PopID();
                ImGui::TextUnformatted(Label, ImGui::FindRenderedTextEnd(Label));
            }
            ImGui::EndGroup();

            return ValueChanged;
        }

        inline bool CheckBox(const char* Label, bool* V) {
            if (!V) return false;

            bool ValueChanged = false;
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0f, 1.0f));
            ValueChanged |= ImGui::Checkbox(Label, V);
            ImGui::PopStyleVar();

            return ValueChanged;
        }
    }  // namespace Controls
}  // namespace Xen::UI