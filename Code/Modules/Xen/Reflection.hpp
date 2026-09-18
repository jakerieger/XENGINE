//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once

#include <Common/XenCommon.hpp>
#include <Common/Math.hpp>

#include "ActorHandle.hpp"
#include "Transform.hpp"

#include <XenPAK/AssetID.hpp>

#include <string>

namespace Xen {
    /// @brief Optional per-property metadata.
    ///
    /// Serialization ignores all of this - it exists so the eventual editor
    /// can build property panels without components having to describe
    /// themselves twice. Declaring it now costs nothing and means components
    /// written today won't need revisiting when the editor arrives.
    /// @brief What an asset reference points at.
    ///
    /// Needed because an AssetID alone says nothing about its content. Without
    /// this, a preload pass that walks reflected assets cannot tell a texture
    /// from a scene or an audio clip, and will happily hand a scene file to
    /// the image decoder.
    enum class AssetKind : u8 {
        /// Unclassified. Preloading skips these - it cannot know which cache
        /// owns them - so they load on demand instead.
        Unknown = 0,
        Texture,
        Audio,
        Scene,
        Mesh,
        Data,
    };

    /// @brief Optional per-property metadata.
    ///
    /// Serialization ignores all of this - it exists so the eventual editor
    /// can build property panels without components having to describe
    /// themselves twice. Declaring it now costs nothing and means components
    /// written today won't need revisiting when the editor arrives.
    struct PropertyMeta {
        const char* DisplayName {nullptr};
        const char* ToolTip {nullptr};
        const char* Category {nullptr};
        f32 Min {0.0f};
        f32 Max {0.0f};
        bool ReadOnly {false};

        AssetKind Asset {AssetKind::Unknown};

        NODISCARD bool HasRange() const { return Min != Max; }
    };

    /// @brief Visitor over a component's or actor's properties.
    ///
    /// Properties are visited by NON-CONST reference, which is what lets a
    /// single Reflect() implementation serve every direction:
    ///   - a saving reflector reads the value out
    ///   - a loading reflector writes a new value in
    ///   - an editor reflector reads it to display and writes it on edit
    ///
    /// Components never learn which is happening.
    class IReflector {
    public:
        virtual ~IReflector() = default;

        /// @brief Reflect a property. Deduces the type and dispatches to the
        /// matching Visit overload; a type with no overload is a compile
        /// error rather than a silent omission.
        template<typename T>
        void Property(const char* Name, T& Value, const PropertyMeta& Meta = {}) {
            Visit(Name, Value, Meta);
        }

        /// @brief Reflect an enum as its underlying integer.
        ///
        /// Enums are common in component data but each one is a distinct type,
        /// so giving IReflector an overload per enum isn't possible. This
        /// round-trips through i32 instead.
        template<typename TEnum>
        void EnumProperty(const char* Name, TEnum& Value, const PropertyMeta& Meta = {}) {
            static_assert(std::is_enum_v<TEnum>, "EnumProperty requires an enum type");
            i32 Raw = CAST<i32>(Value);
            Visit(Name, Raw, Meta);
            Value = CAST<TEnum>(Raw);
        }

        /// @brief True when this reflector is writing values into the object
        /// (loading). Components rarely need this, but it lets one handle
        /// migration or post-load fixups when necessary.
        virtual bool IsLoading() const = 0;

    protected:
        virtual void Visit(const char* Name, bool& Value, const PropertyMeta& Meta)        = 0;
        virtual void Visit(const char* Name, i32& Value, const PropertyMeta& Meta)         = 0;
        virtual void Visit(const char* Name, u32& Value, const PropertyMeta& Meta)         = 0;
        virtual void Visit(const char* Name, u64& Value, const PropertyMeta& Meta)         = 0;
        virtual void Visit(const char* Name, f32& Value, const PropertyMeta& Meta)         = 0;
        virtual void Visit(const char* Name, f64& Value, const PropertyMeta& Meta)         = 0;
        virtual void Visit(const char* Name, std::string& Value, const PropertyMeta& Meta) = 0;
        virtual void Visit(const char* Name, Float2& Value, const PropertyMeta& Meta)      = 0;
        virtual void Visit(const char* Name, Float3& Value, const PropertyMeta& Meta)      = 0;
        virtual void Visit(const char* Name, Float4& Value, const PropertyMeta& Meta)      = 0;
        virtual void Visit(const char* Name, Transform& Value, const PropertyMeta& Meta)   = 0;
        virtual void Visit(const char* Name, Rect& Value, const PropertyMeta& Meta)        = 0;

        /// @brief Actor references serialize as handles, but a raw handle is
        /// only meaningful within one session - slot indices and generations
        /// are assigned at spawn time. A saving reflector must translate this
        /// into something stable (a persistent actor ID) and translate back
        /// on load. Phase 3's scene serializer handles that remapping.
        virtual void Visit(const char* Name, ActorHandle& Value, const PropertyMeta& Meta) = 0;

        /// @brief Asset references get their own overload rather than being
        /// reflected as a raw u64.
        ///
        /// This is what makes an asset dependency visible to tooling: a pass
        /// can walk a scene's reflected properties and collect every asset it
        /// needs, with no component having to declare its dependencies
        /// separately. That drives scene preloading, and later lets an editor
        /// show an asset picker instead of a number field. Reflecting
        /// `MyAsset.Value` instead would make an asset indistinguishable from
        /// any other 64-bit number.
        virtual void Visit(const char* Name, AssetID& Value, const PropertyMeta& Meta) = 0;
    };

    class IReflectable {
    public:
        virtual ~IReflectable() = default;
        virtual void Reflect(IReflector& R) { (void)R; }
    };
}  // namespace Xen