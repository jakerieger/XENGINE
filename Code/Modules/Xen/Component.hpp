//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include "Reflection.hpp"

namespace Xen {
    class Actor;
    class Scene;

    using ComponentTypeID = u64;

/// @brief Declares a component's type identity. Every concrete component must
/// use this in its public section.
///
/// Gives each type a stable name and ID for GetComponent<T> lookups and, in a
/// later phase, for spawning components by name when deserializing a scene.
#define XEN_COMPONENT_TYPE(TypeName)                                                                                   \
    static constexpr const char* StaticTypeName() {                                                                    \
        return #TypeName;                                                                                              \
    }                                                                                                                  \
    static constexpr Xen::ComponentTypeID StaticTypeID() {                                                             \
        return Xen::Hash::FNV1A(#TypeName, Xen::Hash::detail::ConstexprStrLen(#TypeName));                             \
    }                                                                                                                  \
    const char* GetTypeName() const override {                                                                         \
        return StaticTypeName();                                                                                       \
    }                                                                                                                  \
    Xen::ComponentTypeID GetTypeID() const override {                                                                  \
        return StaticTypeID();                                                                                         \
    }

    class IComponent : public IReflectable {
    public:
        ~IComponent() override = default;

        IComponent(const IComponent&)            = delete;
        IComponent& operator=(const IComponent&) = delete;

        virtual const char* GetTypeName() const   = 0;
        virtual ComponentTypeID GetTypeID() const = 0;

        virtual void BeginPlay() {}
        virtual void Tick(const f32 DeltaTime) { (void)DeltaTime; }
        virtual void FixedTick(const f32 FixedDelta) { (void)FixedDelta; }
        virtual void EndPlay() {}

        Actor* GetOwner() const { return _Owner; }
        // Actually implemented in Actor.cpp
        Scene* GetScene() const;
        bool IsEnabled() const { return _Enabled; }
        void SetEnabled(const bool Enabled) { _Enabled = Enabled; }

        template<typename T>
        T* As() {
            ASSERT_BASE_OF(IComponent, T);
            return CAST<T*>(this);
        }

    protected:
        IComponent() = default;

    private:
        friend class Actor;

        Actor* _Owner {nullptr};
        bool _Enabled {true};
        bool _BeganPlay {false};
    };
}  // namespace Xen
