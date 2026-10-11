//
// Created by Jake Rieger on 10/12/2026.
//

#pragma once

#include "Modal.hpp"

#include <array>

namespace Xen {
    class Editor;

    /// Name a new C++ component class to add to the open project.
    class NewComponentClassModal final : public Modal {
    public:
        explicit NewComponentClassModal(Editor& Owner) : _Owner(Owner) {}

        void Open() {
            _ClassName.fill(0);
            RequestOpen();
        }

    protected:
        NODISCARD const char* Title() const override { return "New Component Class"; }
        NODISCARD ImGuiWindowFlags Flags() const override { return ImGuiWindowFlags_None; }
        void DrawBody() override;

    private:
        Editor& _Owner;
        std::array<char, 260> _ClassName {};
    };
}  // namespace Xen
