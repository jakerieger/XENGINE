//
// Created by Jake Rieger on 9/30/2026.
//

#include "PropertyEditor.hpp"
#include "UI.hpp"

#include <Xen/Component.hpp>

#include <Common/Math.hpp>

#include <../../../Vendor/imgui/imgui.h>

#include <cstdio>
#include <cstring>

namespace Xen {
    void PropertyEditorReflector::MaybeDrawCategory(const PropertyMeta& Meta) {
        if (!Meta.Category || _LastCategory == Meta.Category) return;
        _LastCategory = Meta.Category;
        ImGui::SeparatorText(Meta.Category);
    }

    void PropertyEditorReflector::Visit(const char* Name, bool& Value, const PropertyMeta& Meta) {
        MaybeDrawCategory(Meta);
        ImGui::BeginDisabled(Meta.ReadOnly);
        if (ImGui::Checkbox(Name, &Value)) _Edited = true;
        ImGui::EndDisabled();
    }

    void PropertyEditorReflector::Visit(const char* Name, i32& Value, const PropertyMeta& Meta) {
        MaybeDrawCategory(Meta);
        ImGui::BeginDisabled(Meta.ReadOnly);
        // Min/Max default to 0/0 when a property declares no range, which
        // DragInt already treats as "unbounded" (it only clamps when
        // v_min < v_max) - so this needs no HasRange() branch.
        if (ImGui::DragInt(Name, &Value, 1.0f, CAST<i32>(Meta.Min), CAST<i32>(Meta.Max))) _Edited = true;
        ImGui::EndDisabled();
    }

    void PropertyEditorReflector::Visit(const char* Name, u32& Value, const PropertyMeta& Meta) {
        MaybeDrawCategory(Meta);
        ImGui::BeginDisabled(Meta.ReadOnly);
        // DragScalar takes real pointers for its bounds (unlike DragInt/
        // DragFloat's own min==max-means-unbounded convenience), so this
        // one does need the explicit HasRange() branch.
        u32 Min = CAST<u32>(Meta.Min), Max = CAST<u32>(Meta.Max);
        const bool Changed = ImGui::DragScalar(Name,
                                               ImGuiDataType_U32,
                                               &Value,
                                               1.0f,
                                               Meta.HasRange() ? &Min : nullptr,
                                               Meta.HasRange() ? &Max : nullptr);
        if (Changed) _Edited = true;
        ImGui::EndDisabled();
    }

    void PropertyEditorReflector::Visit(const char* Name, u64& Value, const PropertyMeta& Meta) {
        MaybeDrawCategory(Meta);
        ImGui::BeginDisabled(Meta.ReadOnly);
        u64 Min = CAST<u64>(Meta.Min), Max = CAST<u64>(Meta.Max);
        const bool Changed = ImGui::DragScalar(Name,
                                               ImGuiDataType_U64,
                                               &Value,
                                               1.0f,
                                               Meta.HasRange() ? &Min : nullptr,
                                               Meta.HasRange() ? &Max : nullptr);
        if (Changed) _Edited = true;
        ImGui::EndDisabled();
    }

    void PropertyEditorReflector::Visit(const char* Name, f32& Value, const PropertyMeta& Meta) {
        MaybeDrawCategory(Meta);
        ImGui::BeginDisabled(Meta.ReadOnly);
        // Same unbounded-when-Min==Max convenience as DragInt - this is
        // exactly why PBRMaterialComponent's Metallic/Roughness/AO ranges
        // (0..1) just work here with no special-casing.
        if (ImGui::DragFloat(Name, &Value, 0.01f, Meta.Min, Meta.Max)) _Edited = true;
        ImGui::EndDisabled();
    }

    void PropertyEditorReflector::Visit(const char* Name, f64& Value, const PropertyMeta& Meta) {
        MaybeDrawCategory(Meta);
        ImGui::BeginDisabled(Meta.ReadOnly);
        double Min = Meta.Min, Max = Meta.Max;
        const bool Changed = ImGui::DragScalar(Name,
                                               ImGuiDataType_Double,
                                               &Value,
                                               0.01f,
                                               Meta.HasRange() ? &Min : nullptr,
                                               Meta.HasRange() ? &Max : nullptr,
                                               "%.6f");
        if (Changed) _Edited = true;
        ImGui::EndDisabled();
    }

    void PropertyEditorReflector::Visit(const char* Name, std::string& Value, const PropertyMeta& Meta) {
        MaybeDrawCategory(Meta);

        // A fixed editor-only buffer, not a general string editor - long
        // enough for the short identifiers (names, submesh names, ...)
        // every current component reflects as a string. Something that
        // needs to hold more than this needs its own widget, not a bigger
        // buffer here.
        constexpr size_t BufferSize = 256;
        char Buffer[BufferSize];
        strncpy_s(Buffer, Value.c_str(), BufferSize - 1);

        ImGui::BeginDisabled(Meta.ReadOnly);
        if (ImGui::InputText(Name, Buffer, BufferSize)) {
            Value   = Buffer;
            _Edited = true;
        }
        ImGui::EndDisabled();
    }

    void PropertyEditorReflector::Visit(const char* Name, Float2& Value, const PropertyMeta& Meta) {
        MaybeDrawCategory(Meta);
        ImGui::BeginDisabled(Meta.ReadOnly);
        if (ImGui::DragFloat2(Name, &Value.x, 0.01f, Meta.Min, Meta.Max)) _Edited = true;
        ImGui::EndDisabled();
    }

    void PropertyEditorReflector::Visit(const char* Name, Float3& Value, const PropertyMeta& Meta) {
        MaybeDrawCategory(Meta);
        ImGui::BeginDisabled(Meta.ReadOnly);
        if (ImGui::DragFloat3(Name, &Value.x, 0.01f, Meta.Min, Meta.Max)) _Edited = true;
        ImGui::EndDisabled();
    }

    void PropertyEditorReflector::Visit(const char* Name, Float4& Value, const PropertyMeta& Meta) {
        MaybeDrawCategory(Meta);
        ImGui::BeginDisabled(Meta.ReadOnly);
        if (ImGui::DragFloat4(Name, &Value.x, 0.01f, Meta.Min, Meta.Max)) _Edited = true;
        ImGui::EndDisabled();
    }

    void PropertyEditorReflector::Visit(const char* Name, Transform& Value, const PropertyMeta& Meta) {
        MaybeDrawCategory(Meta);
        ImGui::PushID(Name);
        ImGui::BeginDisabled(Meta.ReadOnly);

        if (ImGui::DragFloat3("Position", &Value.Position.x, 0.01f)) _Edited = true;

        // Same round trip View_Inspector already does for the actor's own
        // Transform (see its own comment there) - Rotation is a quaternion
        // internally, but nobody wants to drag quaternion components by
        // hand.
        const Float3 EulerRadians = QuaternionToEuler(Value.Rotation);
        Float3 EulerDegrees       = {DirectX::XMConvertToDegrees(EulerRadians.x),
                                     DirectX::XMConvertToDegrees(EulerRadians.y),
                                     DirectX::XMConvertToDegrees(EulerRadians.z)};
        if (ImGui::DragFloat3("Rotation", &EulerDegrees.x, 0.1f)) {
            Value.Rotation = EulerToQuaternion({DirectX::XMConvertToRadians(EulerDegrees.x),
                                                DirectX::XMConvertToRadians(EulerDegrees.y),
                                                DirectX::XMConvertToRadians(EulerDegrees.z)});
            _Edited        = true;
        }

        if (ImGui::DragFloat3("Scale", &Value.Scale.x, 0.01f)) _Edited = true;

        ImGui::EndDisabled();
        ImGui::PopID();
    }

    void PropertyEditorReflector::Visit(const char* Name, Rect& Value, const PropertyMeta& Meta) {
        MaybeDrawCategory(Meta);
        ImGui::BeginDisabled(Meta.ReadOnly);
        // Rect's four f32 members (X, Y, Width, Height) are laid out
        // contiguously with no padding - same "&V.x as a float[N]" trick
        // View_Inspector already relies on for Float3 - so one DragFloat4
        // row edits all four.
        if (ImGui::DragFloat4(Name, &Value.X, 0.01f)) _Edited = true;
        ImGui::EndDisabled();
    }

    void PropertyEditorReflector::Visit(const char* Name, ActorHandle& Value, const PropertyMeta&) {
        MaybeDrawCategory({});
        // No actor picker exists yet, so this is read-only until one does
        // - editing an actor reference means resolving another actor to
        // point at, not typing a number.
        char Buffer[64];
        std::snprintf(Buffer, sizeof(Buffer), "Actor #%u (gen %u)", Value.Index, Value.Generation);
        ImGui::BeginDisabled(true);
        ImGui::InputText(Name, Buffer, sizeof(Buffer));
        ImGui::EndDisabled();
    }

    void PropertyEditorReflector::Visit(const char* Name, AssetID& Value, const PropertyMeta& Meta) {
        MaybeDrawCategory(Meta);
        // Meta.Asset says what KIND of asset this is meant to be; the slot
        // only accepts a dragged content browser asset of that kind.
        const AssetIndex Empty;
        ImGui::BeginDisabled(Meta.ReadOnly);
        AssetID Picked = Value;
        if (UI::Controls::AssetSlot(Name, Picked, Meta.Asset, _Assets ? *_Assets : Empty)) {
            if (!_Owner || !_Owner->SetAssetProperty(Name, Picked)) Value = Picked;
            _Edited = true;
        }
        ImGui::EndDisabled();
    }
}  // namespace Xen
