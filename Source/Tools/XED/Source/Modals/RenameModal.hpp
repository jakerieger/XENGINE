//
// Created by Jake Rieger on 10/12/2026.
//

#pragma once

#include "Modal.hpp"

#include <Common/Platform.hpp>

#include <functional>
#include <string>

namespace Xen {
    /// Rename a file or folder on disk. A file keeps its extension (it decides
    /// what kind of asset the file is); only the name before it is edited.
    class RenameModal final : public Modal {
    public:
        using Callback = std::function<void(const fs::path& NewPath)>;

        /// @param OnRenamed called with the new path once the rename has
        /// happened.
        void Open(const fs::path& Target, bool IsDirectory, Callback OnRenamed);

    protected:
        NODISCARD const char* Title() const override { return "Rename"; }
        void DrawBody() override;

    private:
        fs::path _Target;
        bool _IsDirectory {false};
        Callback _OnRenamed;
        char _Buffer[256] {};
        std::string _Error;
    };
}  // namespace Xen
