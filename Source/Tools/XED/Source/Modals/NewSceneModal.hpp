//
// Created by Jake Rieger on 10/12/2026.
//

#pragma once

#include "Modal.hpp"

#include <array>

namespace Xen {
    class Editor;

    /// Name a new scene, created in the project's Content/scenes folder.
    class NewSceneModal final : public Modal {
    public:
        explicit NewSceneModal(Editor& Owner) : _Owner(Owner) {}

        void Open() { RequestOpen(); }

    protected:
        NODISCARD const char* Title() const override { return "New Scene"; }
        NODISCARD ImGuiWindowFlags Flags() const override { return ImGuiWindowFlags_None; }
        void DrawBody() override;

    private:
        Editor& _Owner;
        std::array<char, 260> _SceneName {};
    };
}  // namespace Xen
