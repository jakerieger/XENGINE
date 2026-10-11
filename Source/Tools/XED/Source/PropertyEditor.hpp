//
// Created by Jake Rieger on 9/30/2026.
//
// An IReflector that draws Dear ImGui edit controls instead of reading or
// writing JSON - the third leg of IReflector's own "save / load / edit"
// comment (see Xen/Reflection.hpp). A component never learns which
// reflector is visiting it, so Inspector property editors come for free
// from the exact same Reflect() every component already wrote for
// SceneSerializer - nothing here duplicates a component's own property
// list.
//
// Deliberately lives in XED, not the shared Xen engine library: this is
// the same "ImGui stays out of the engine" boundary EditorUI/DebugUI's own
// separation already established (see EditorUI.hpp's file comment) - an
// IReflector override that calls Dear ImGui functions has no business in
// a module standalone games link into.

#pragma once

#include <Xen/Reflection.hpp>

namespace Xen {
    class AssetIndex;
    class IComponent;

    class PropertyEditorReflector final : public IReflector {
    public:
        /// @param Assets resolves asset IDs to names for asset slots; must
        /// outlive this reflector.
        /// @param Owner the component being reflected, if any: an edited asset
        /// property goes through its SetAssetProperty so a live scene picks
        /// the change up (see IComponent::SetAssetProperty).
        explicit PropertyEditorReflector(const AssetIndex* Assets, IComponent* Owner = nullptr)
            : _Assets(Assets), _Owner(Owner) {}

        // Reads Value to display it and writes back through the same
        // reference on edit, both in one Visit call - neither "loading" nor
        // "saving" in SceneSerializer's sense, so this is a judgment call.
        // False here (a per-frame read that also happens to write on
        // interaction) since nothing in the codebase currently branches on
        // IsLoading() to change behavior either way (checked: only
        // SceneSerializer's own reflectors and AssetPreloader do).
        bool IsLoading() const override { return false; }

        /// @brief True if the last Reflect() call changed anything - lets a
        /// caller know to e.g. mark a scene dirty once that concept exists.
        NODISCARD bool WasEdited() const { return _Edited; }
        void ResetEdited() { _Edited = false; }

    protected:
        void Visit(const char* Name, bool& Value, const PropertyMeta& Meta) override;
        void Visit(const char* Name, i32& Value, const PropertyMeta& Meta) override;
        void Visit(const char* Name, u32& Value, const PropertyMeta& Meta) override;
        void Visit(const char* Name, u64& Value, const PropertyMeta& Meta) override;
        void Visit(const char* Name, f32& Value, const PropertyMeta& Meta) override;
        void Visit(const char* Name, f64& Value, const PropertyMeta& Meta) override;
        void Visit(const char* Name, std::string& Value, const PropertyMeta& Meta) override;
        void Visit(const char* Name, Float2& Value, const PropertyMeta& Meta) override;
        void Visit(const char* Name, Float3& Value, const PropertyMeta& Meta) override;
        void Visit(const char* Name, Float4& Value, const PropertyMeta& Meta) override;
        void Visit(const char* Name, Transform& Value, const PropertyMeta& Meta) override;
        void Visit(const char* Name, Rect& Value, const PropertyMeta& Meta) override;
        void Visit(const char* Name, ActorHandle& Value, const PropertyMeta& Meta) override;
        void Visit(const char* Name, AssetID& Value, const PropertyMeta& Meta) override;

    private:
        // Draws a SeparatorText("Category") row the first time a new
        // PropertyMeta::Category is seen - properties within one component
        // are already declared grouped by category (see e.g.
        // PBRMaterialComponent::Reflect), so a simple "did the category
        // change since the last property" check is enough to get Unity-
        // style sub-headers with no extra bookkeeping in the component
        // itself.
        void MaybeDrawCategory(const PropertyMeta& Meta);

        // Set by every Visit override that actually changed Value, and
        // read back by WasEdited() - see its own comment.
        bool _Edited {false};
        const AssetIndex* _Assets {nullptr};
        IComponent* _Owner {nullptr};
        std::string _LastCategory;
    };
}  // namespace Xen
