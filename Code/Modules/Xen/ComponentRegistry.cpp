//
// Created by Jake Rieger on 9/8/2026.
//

#include "ComponentRegistry.hpp"

#include <algorithm>
#include <ranges>

namespace Xen {
    ComponentRegistry& ComponentRegistry::Get() {
        static ComponentRegistry Instance;
        return Instance;
    }

    std::unique_ptr<IComponent> ComponentRegistry::Create(const std::string& TypeName) const {
        const auto It = _NameToID.find(TypeName);
        if (It == _NameToID.end()) return nullptr;
        return Create(It->second);
    }

    std::unique_ptr<IComponent> ComponentRegistry::Create(const ComponentTypeID ID) const {
        const auto It = _Types.find(ID);
        if (It == _Types.end()) return nullptr;
        return It->second.Factory();
    }

    bool ComponentRegistry::IsRegistered(const std::string& TypeName) const {
        return _NameToID.contains(TypeName);
    }

    bool ComponentRegistry::IsRegistered(const ComponentTypeID ID) const {
        return _Types.contains(ID);
    }

    const ComponentRegistry::TypeInfo* ComponentRegistry::FindType(const ComponentTypeID ID) const {
        const auto It = _Types.find(ID);
        return It != _Types.end() ? &It->second : nullptr;
    }

    std::vector<std::string> ComponentRegistry::GetRegisteredNames() const {
        std::vector<std::string> Out;
        Out.reserve(_NameToID.size());
        for (const auto& Name : _NameToID | std::views::keys)
            Out.push_back(Name);
        std::ranges::sort(Out);
        return Out;
    }

    void ComponentRegistry::Clear() {
        _Types.clear();
        _NameToID.clear();
    }

    void ComponentRegistry::RegisterInternal(const char* Name, ComponentTypeID ID, FactoryFn Factory) {
        if (const auto It = _Types.find(ID); It != _Types.end()) {
            if (It->second.Name == Name) return;

            THROW_ENGINE_EXCEPTION(
              EngineException,
              std::format("component type ID collision between '{}' and '{}'", It->second.Name, Name));
        }

        if (const auto It = _NameToID.find(Name); It != _NameToID.end()) {
            THROW_ENGINE_EXCEPTION(
              EngineException,
              std::format("component name '{}' is already registered to a different type ID", Name));
        }

        _NameToID[Name] = ID;
        _Types.emplace(ID,
                       TypeInfo {
                         .Name    = Name,
                         .ID      = ID,
                         .Factory = std::move(Factory),
                       });
    }
}  // namespace Xen