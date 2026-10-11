//
// Created by Jake Rieger on 10/13/2026.
//

#pragma once

#include "Modal.hpp"
#include "../ActorPresets.hpp"

#include <array>

namespace Xen {
    class Editor;

    /// Create an actor in the open scene: empty, or one of the ready-made
    /// kinds (camera, lights, environment, ...) from ActorPresets.
    class AddActorModal final : public Modal {
    public:
        explicit AddActorModal(Editor& Owner) : _Owner(Owner) {}

        void Open();

    protected:
        NODISCARD const char* Title() const override { return "Add Actor"; }
        void DrawBody() override;

    private:
        void SelectPreset(ActorPreset Preset);

        Editor& _Owner;
        ActorPreset _Preset {ActorPreset::Empty};
        std::array<char, 260> _Name {};
        // Once the name's been typed over, picking another kind keeps it
        // rather than swapping in that kind's default.
        bool _NameEdited {false};
    };
}  // namespace Xen
