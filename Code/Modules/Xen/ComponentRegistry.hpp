//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include "Component.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Xen {
    class ComponentRegistry {
    public:
        using FactoryFn = std::function<std::unique_ptr<IComponent>()>;

        struct TypeInfo {
            std::string Name;
            ComponentTypeID ID {0};
            FactoryFn Factory;
        };

        static ComponentRegistry& Get();

        template<typename T>
        void Register() {
            static_assert(std::is_base_of_v<IComponent, T>, "T must derive from Component");
            static_assert(std::is_default_constructible_v<T>,
                          "Registered components must be default-constructible - the loader "
                          "creates them empty and then fills them in via Reflect()");

            RegisterInternal(T::StaticTypeName(), T::StaticTypeID(), [] {
                return std::unique_ptr<IComponent>(new T());
            });
        }

        _NoDiscard std::unique_ptr<IComponent> Create(const std::string& TypeName) const;
        _NoDiscard std::unique_ptr<IComponent> Create(ComponentTypeID ID) const;
        _NoDiscard bool IsRegistered(const std::string& TypeName) const;
        _NoDiscard bool IsRegistered(ComponentTypeID ID) const;
        _NoDiscard const TypeInfo* FindType(ComponentTypeID ID) const;
        _NoDiscard std::vector<std::string> GetRegisteredNames() const;
        _NoDiscard size_t GetTypeCount() const { return _Types.size(); }

        void Clear();

    private:
        void RegisterInternal(const char* Name, ComponentTypeID ID, FactoryFn Factory);

        std::unordered_map<ComponentTypeID, TypeInfo> _Types;
        std::unordered_map<std::string, ComponentTypeID> _NameToID;
    };
}  // namespace Xen

#define _DefineComponent(Type)                                                                                         \
    class Type;                                                                                                        \
    namespace {                                                                                                        \
        const struct Type##_AutoRegister {                                                                             \
            Type##_AutoRegister() { Xen::ComponentRegistry::Get().Register<Type>(); }                                  \
        } g_##Type##_AutoRegister;                                                                                     \
    }