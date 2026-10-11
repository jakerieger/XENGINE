//
// Created by Jake Rieger on 10/3/2026.
//

#pragma once

#include "Component.hpp"

#include <nlohmann/json.hpp>

#include <string>

namespace Xen {
    /// @brief Stands in for a component whose type isn't registered - typically
    /// one from a game module that isn't loaded (not built yet, failed to
    /// load) or that no longer defines it.
    ///
    /// It keeps the type name and the raw saved properties, and the scene
    /// serializer writes them back unchanged, so opening and saving a scene
    /// without the module can't lose the data. It does nothing at runtime.
    ///
    /// Never registered: it can't be created by name, only by the serializer.
    class PlaceholderComponent final : public IComponent {
    public:
        PlaceholderComponent(std::string TypeName, nlohmann::ordered_json Properties)
            : _TypeName(std::move(TypeName)), _Properties(std::move(Properties)) {}

        const char* GetTypeName() const override { return _TypeName.c_str(); }
        Xen::ComponentTypeID GetTypeID() const override {
            return Xen::Hash::FNV1A(_TypeName.c_str(), _TypeName.size());
        }

        NODISCARD const nlohmann::ordered_json& GetProperties() const { return _Properties; }

    private:
        std::string _TypeName;
        nlohmann::ordered_json _Properties;
    };
}  // namespace Xen
