//
// Created by Jake Rieger on 10/3/2026.
//

#pragma once

#include <../../../Vendor/imgui/imgui.h>
#include <../../../Vendor/imgui/imgui_internal.h>

#include <Xen/Color.hpp>

#include "AssetIndex.hpp"

#include <cstdio>

namespace Xen::UI {
    /// Payload type for dragging an asset between panels (content browser ->
    /// inspector asset slots).
    inline constexpr const char* kAssetPayloadType = "XED_ASSET";

    struct AssetDragPayload {
        u64 ID;
        AssetKind Kind;
    };

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

        inline bool
        ToolbarButton(const char* ID, const ImTextureID Icon, const ImVec2 IconSize, const bool Enabled = true) {
            bool Clicked = false;

            ImGui::BeginDisabled(!Enabled);
            {
                if (Icon != 0) {
                    Clicked = ImGui::ImageButton(ID, Icon, IconSize);
                } else {
                    ImGui::Dummy(IconSize);
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();

            return Clicked;
        }

        /// Starts a two-column settings table: property names on the left,
        /// editors on the right, in a 1:2 width ratio. Rows are added with
        /// SettingsRow / SettingsPathRow. Always pair with EndSettingsTable
        /// when this returns true.
        inline bool BeginSettingsTable(const char* ID) {
            if (!ImGui::BeginTable(ID, 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_PadOuterX)) return false;
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 2.0f);
            return true;
        }

        inline void EndSettingsTable() {
            ImGui::EndTable();
        }

        /// Starts a settings row: draws Label in the left column and leaves the
        /// right column current, with the next item set to fill it. Draw the
        /// editor right after, with an "##id" label.
        inline void SettingsRow(const char* Label) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(Label);
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
        }

        /// A settings row for a file path: a read-only field filling the right
        /// column, minus a "..." browse button. Returns true when browse is
        /// clicked; the caller opens its dialog and writes into Buffer.
        inline bool SettingsPathRow(const char* Label, char* Buffer, const size_t BufferSize) {
            ImGui::PushID(Label);
            SettingsRow(Label);

            const ImGuiStyle& Style = ImGui::GetStyle();
            const f32 ButtonWidth   = ImGui::CalcTextSize("...").x + Style.FramePadding.x * 2.0f;
            ImGui::SetNextItemWidth(-(ButtonWidth + Style.ItemSpacing.x));
            ImGui::InputText("##path", Buffer, BufferSize, ImGuiInputTextFlags_ReadOnly);
            ImGui::SameLine();
            const bool Browse = ImGui::Button("...", ImVec2(ButtonWidth, 0.0f));
            ImGui::PopID();

            return Browse;
        }

        /// A property field for an asset reference: shows the asset's name and
        /// accepts a drag from the content browser, but only of the Accepts
        /// kind (AssetKind::Unknown accepts any). An incompatible drag
        /// outlines the field red and is ignored on release. The X button
        /// clears the reference. Returns true if Value changed.
        inline bool AssetSlot(const char* Label, AssetID& Value, const AssetKind Accepts, const AssetIndex& Index) {
            const ImGuiContext& Ctx = *GImGui;
            bool Changed            = false;

            ImGui::PushID(Label);
            ImGui::BeginGroup();

            const f32 ClearWidth = ImGui::GetFrameHeight();
            const f32 SlotWidth  = ImGui::CalcItemWidth() - ClearWidth - Ctx.Style.ItemInnerSpacing.x;

            const AssetEntry* Entry = Index.Find(Value);
            char Text[320];
            if (!Value.IsValid()) {
                std::snprintf(Text, sizeof(Text), "None (%s)###slot", AssetKindName(Accepts));
            } else if (Entry) {
                std::snprintf(Text, sizeof(Text), "%s###slot", Entry->Name.c_str());
            } else {
                std::snprintf(Text, sizeof(Text), "Missing (0x%016llX)###slot", CAST<unsigned long long>(Value.Value));
            }

            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
            if (!Value.IsValid() || !Entry)
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::Button(Text, ImVec2(SlotWidth, 0.0f));
            if (!Value.IsValid() || !Entry) ImGui::PopStyleColor();
            ImGui::PopStyleVar();

            if (Entry && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
                ImGui::SetTooltip("%s", Entry->RelativePath.generic_string().c_str());
            }

            if (ImGui::BeginDragDropTarget()) {
                // AcceptBeforeDelivery gives us the payload while it's still
                // being dragged, so the kind can be checked (and the field
                // outlined) before the mouse is released.
                const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(
                  kAssetPayloadType,
                  ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
                if (Payload && Payload->DataSize == sizeof(AssetDragPayload)) {
                    const auto& Dropped   = *CAST<const AssetDragPayload*>(Payload->Data);
                    const bool Compatible = Accepts == AssetKind::Unknown || Dropped.Kind == Accepts;

                    const ImU32 Outline = Compatible ? IM_COL32(131, 203, 16, 255) : IM_COL32(235, 55, 81, 255);
                    ImGui::GetForegroundDrawList()->AddRect(ImGui::GetItemRectMin(),
                                                            ImGui::GetItemRectMax(),
                                                            Outline,
                                                            Ctx.Style.FrameRounding,
                                                            0,
                                                            2.0f);
                    if (!Compatible) ImGui::SetTooltip("Requires a %s asset", AssetKindName(Accepts));

                    if (Compatible && Payload->IsDelivery() && Value.Value != Dropped.ID) {
                        Value   = AssetID(Dropped.ID);
                        Changed = true;
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::SameLine(0.0f, Ctx.Style.ItemInnerSpacing.x);
            ImGui::BeginDisabled(!Value.IsValid());
            if (ImGui::Button("X", ImVec2(ClearWidth, 0.0f))) {
                Value   = AssetID();
                Changed = true;
            }
            ImGui::EndDisabled();

            ImGui::SameLine(0.0f, Ctx.Style.ItemInnerSpacing.x);
            ImGui::TextUnformatted(Label, ImGui::FindRenderedTextEnd(Label));

            ImGui::EndGroup();
            ImGui::PopID();

            return Changed;
        }

        /// Makes the last item (typically an AssetTile) draggable as an asset.
        inline void AssetDragSource(const AssetEntry& Entry) {
            if (!ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) return;

            const AssetDragPayload Payload {Entry.ID.Value, Entry.Kind};
            ImGui::SetDragDropPayload(kAssetPayloadType, &Payload, sizeof(Payload));
            ImGui::TextUnformatted(Entry.Name.c_str());
            ImGui::EndDragDropSource();
        }

        struct AssetTileResult {
            bool Clicked {false};
            bool DoubleClicked {false};
        };

        /// One cell of a thumbnail grid: Icon (square, Size px) over a
        /// single-line, clipped Label. Draws the selected/hover highlight.
        /// The tile is the last item afterwards, so a drag source or context
        /// menu can attach to it.
        inline AssetTileResult
        AssetTile(const char* ID, const ImTextureID Icon, const char* Label, const f32 Size, const bool Selected) {
            const ImGuiContext& Ctx = *GImGui;
            const f32 LabelHeight   = ImGui::GetTextLineHeight() + Ctx.Style.FramePadding.y;

            AssetTileResult Result;
            ImGui::PushID(ID);
            const ImVec2 CellSize(Size, Size + LabelHeight);

            Result.Clicked = ImGui::Selectable("##tile", Selected, ImGuiSelectableFlags_AllowDoubleClick, CellSize);
            Result.DoubleClicked = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

            const ImVec2 Min = ImGui::GetItemRectMin();
            const ImVec2 Max = ImGui::GetItemRectMax();
            ImDrawList* Draw = ImGui::GetWindowDrawList();

            const f32 Pad = Size * 0.06f;
            if (Icon != 0) { Draw->AddImage(Icon, {Min.x + Pad, Min.y + Pad}, {Max.x - Pad, Min.y + Size - Pad}); }
            ImGui::RenderTextClipped({Min.x + 2.0f, Min.y + Size},
                                     {Max.x - 2.0f, Max.y},
                                     Label,
                                     nullptr,
                                     nullptr,
                                     ImVec2(0.5f, 0.0f));

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) ImGui::SetTooltip("%s", Label);
            ImGui::PopID();

            return Result;
        }

        inline bool TransparentButton(const char* ID) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4 {0.f, 0.f, 0.f, 0.f});
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4 {0.f, 0.f, 0.f, 0.f});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4 {1.f, 1.f, 1.f, 0.05f});

            const auto Clicked = ImGui::Button(ID);

            ImGui::PopStyleColor(3);

            return Clicked;
        }
    }  // namespace Controls
}  // namespace Xen::UI