//
// Created by Jake Rieger on 9/8/2026.
//

#include "ComponentRegistry.hpp"

#include <algorithm>
#include <format>
#include <ranges>
#include <utility>

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

    void ComponentRegistry::BeginModuleScope(const ModuleID Module) {
        if (_ScopeModule != NoModule) {
            THROW_ENGINE_EXCEPTION(EngineException,
                                   std::format("module scope {} opened while scope {} is still open",
                                               Module,
                                               _ScopeModule));
        }
        _ScopeModule = Module;
        _ScopeErrors.clear();
    }

    std::vector<std::string> ComponentRegistry::EndModuleScope() {
        _ScopeModule = NoModule;
        return std::exchange(_ScopeErrors, {});
    }

    size_t ComponentRegistry::UnregisterModule(const ModuleID Module) {
        if (Module == NoModule) return 0;

        size_t Removed = 0;
        for (auto It = _Types.begin(); It != _Types.end();) {
            if (It->second.Owner == Module) {
                _NameToID.erase(It->second.Name);
                It = _Types.erase(It);
                ++Removed;
            } else {
                ++It;
            }
        }
        return Removed;
    }

    size_t ComponentRegistry::GetModuleTypeCount(const ModuleID Module) const {
        return CAST<size_t>(std::ranges::count_if(_Types | std::views::values,
                                                  [Module](const TypeInfo& T) { return T.Owner == Module; }));
    }

    void ComponentRegistry::RegisterInternal(const char* Name, ComponentTypeID ID, FactoryFn Factory) {
        // See BeginModuleScope: inside a module load, record rather than throw.
        const auto Fail = [this](std::string Message) {
            if (_ScopeModule != NoModule) {
                _ScopeErrors.push_back(std::move(Message));
                return;
            }
            THROW_ENGINE_EXCEPTION(EngineException, Message);
        };

        if (const auto It = _Types.find(ID); It != _Types.end()) {
            // Same type registered again - every translation unit that includes
            // a component's header runs its XEN_COMPONENT registrar, so this
            // is the normal case (a module including engine component headers
            // included). The first registration wins and keeps its owner...
            //
            // ...unless that owner is a *different* module: then this is a
            // module still registered from an earlier load (its stale Factory
            // would point into an unloaded DLL), or two modules defining the
            // same component - both bugs the loader needs to hear about.
            if (It->second.Name == Name) {
                if (_ScopeModule != NoModule && It->second.Owner != NoModule && It->second.Owner != _ScopeModule) {
                    Fail(std::format("component '{}' is already registered by module {} "
                                     "(was it unregistered before reloading?)",
                                     Name,
                                     It->second.Owner));
                }
                return;
            }

            return Fail(std::format("component type ID collision between '{}' and '{}'", It->second.Name, Name));
        }

        if (const auto It = _NameToID.find(Name); It != _NameToID.end()) {
            return Fail(std::format("component name '{}' is already registered to a different type ID", Name));
        }

        _NameToID[Name] = ID;
        _Types.emplace(ID,
                       TypeInfo {
                         .Name    = Name,
                         .ID      = ID,
                         .Factory = std::move(Factory),
                         .Owner   = _ScopeModule,
                       });
    }
}  // namespace Xen