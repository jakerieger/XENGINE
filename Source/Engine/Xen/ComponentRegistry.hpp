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
    /// @brief Identifies a loaded game module (see GameModule.hpp) as the owner
    /// of the component types it registered. 0 means "not a module": the
    /// engine's own types and those of a game linked directly into its exe.
    using ModuleID = u32;
    constexpr ModuleID NoModule = 0;

    class ComponentRegistry {
    public:
        using FactoryFn = std::function<std::unique_ptr<IComponent>()>;

        struct TypeInfo {
            std::string Name;
            ComponentTypeID ID {0};
            FactoryFn Factory;
            /// Which module's code Factory lives in. Its types must be removed
            /// (UnregisterModule) before that module's DLL is unloaded.
            ModuleID Owner {NoModule};
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

        NODISCARD std::unique_ptr<IComponent> Create(const std::string& TypeName) const;
        NODISCARD std::unique_ptr<IComponent> Create(ComponentTypeID ID) const;
        NODISCARD bool IsRegistered(const std::string& TypeName) const;
        NODISCARD bool IsRegistered(ComponentTypeID ID) const;
        NODISCARD const TypeInfo* FindType(ComponentTypeID ID) const;
        NODISCARD std::vector<std::string> GetRegisteredNames() const;
        NODISCARD size_t GetTypeCount() const { return _Types.size(); }

        void Clear();

        /// @brief Tags every registration until EndModuleScope() as owned by
        /// Module. A module's XEN_COMPONENT registrations run as static
        /// initializers inside LoadLibrary, before the host can call into it -
        /// so the loader opens a scope around the load itself. Scopes don't
        /// nest.
        ///
        /// Inside a scope, a conflicting registration is recorded instead of
        /// thrown: it runs during DLL initialization, where an escaping
        /// exception would take the whole process down. EndModuleScope returns
        /// those errors, and the loader rejects the module if there are any.
        void BeginModuleScope(ModuleID Module);
        NODISCARD std::vector<std::string> EndModuleScope();

        /// @brief Removes every type Module registered, returning how many.
        /// Must run before the module's DLL is unloaded: each type's Factory
        /// is code inside that DLL. Instances created from those types must
        /// already be destroyed too - their vtables live there as well.
        size_t UnregisterModule(ModuleID Module);

        NODISCARD size_t GetModuleTypeCount(ModuleID Module) const;

    private:
        void RegisterInternal(const char* Name, ComponentTypeID ID, FactoryFn Factory);

        std::unordered_map<ComponentTypeID, TypeInfo> _Types;
        std::unordered_map<std::string, ComponentTypeID> _NameToID;
        ModuleID _ScopeModule {NoModule};
        std::vector<std::string> _ScopeErrors;
    };
}  // namespace Xen

#define XEN_COMPONENT(Type)                                                                                       \
    class Type;                                                                                                        \
    namespace {                                                                                                        \
        const struct Type##_AutoRegister {                                                                             \
            Type##_AutoRegister() { Xen::ComponentRegistry::Get().Register<Type>(); }                                  \
        } g_##Type##_AutoRegister;                                                                                     \
    }