//
// Created by Jake Rieger on 10/12/2026.
//

#pragma once

#include "Modal.hpp"

#include <Xen/ActorHandle.hpp>

#include <array>
#include <string>

namespace Xen {
    class Editor;

    /// Name a prefab and save an actor (and its children) as
    /// Content/prefabs/<name>.xprefab.
    class SaveAsPrefabModal final : public Modal {
    public:
        explicit SaveAsPrefabModal(Editor& Owner) : _Owner(Owner) {}

        /// @param Source the actor to save; DefaultName is what the name
        /// field starts as (normally the actor's own).
        void Open(ActorHandle Source, const std::string& DefaultName);

    protected:
        NODISCARD const char* Title() const override { return "Save as Prefab"; }
        void DrawBody() override;

    private:
        Editor& _Owner;
        ActorHandle _Source {};
        std::array<char, 260> _Name {};
        std::string _Error;
    };
}  // namespace Xen
