//
// Created by Jake Rieger on 10/12/2026.
//

#pragma once

#include "Modal.hpp"

#include <array>

namespace Xen {
    /// Type an asset's path (relative to the content root) to see the AssetID
    /// the engine derives from it - the hash of its canonical path.
    class GenerateAssetIDModal final : public Modal {
    public:
        void Open() { RequestOpen(); }

    protected:
        NODISCARD const char* Title() const override { return "Generate Asset ID"; }
        NODISCARD ImGuiWindowFlags Flags() const override { return ImGuiWindowFlags_None; }
        void DrawBody() override;

    private:
        std::array<char, 512> _AssetPath {};
        std::array<char, 512> _GeneratedID {};
    };
}  // namespace Xen
