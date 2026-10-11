//
// Created by Jake Rieger on 10/12/2026.
//

#pragma once

#include "Modal.hpp"

#include <string>

namespace Xen {
    class Editor;

    /// Pick a component type to add to the selected actor.
    class AddComponentModal final : public Modal {
    public:
        explicit AddComponentModal(Editor& Owner) : _Owner(Owner) {}

        void Open() {
            _Choice.clear();
            RequestOpen();
        }

    protected:
        NODISCARD const char* Title() const override { return "Add Component"; }
        void DrawBody() override;

    private:
        Editor& _Owner;
        std::string _Choice;
    };
}  // namespace Xen
