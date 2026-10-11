//
// Created by Jake Rieger on 10/12/2026.
//

#pragma once

#include "Modal.hpp"

#include <array>

namespace Xen {
    class Editor;

    /// Name and place a new project, create it, and offer to open it.
    class NewProjectModal final : public Modal {
    public:
        explicit NewProjectModal(Editor& Owner) : _Owner(Owner) {}

        void Open() { RequestOpen(); }

    protected:
        NODISCARD const char* Title() const override { return "New Project"; }
        NODISCARD ImGuiWindowFlags Flags() const override { return ImGuiWindowFlags_None; }
        void DrawBody() override;

    private:
        void Reset();

        Editor& _Owner;
        std::array<char, 260> _ProjectName {};
        std::array<char, 260> _ProjectDir {};
    };
}  // namespace Xen
