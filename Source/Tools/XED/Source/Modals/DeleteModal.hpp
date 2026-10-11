//
// Created by Jake Rieger on 10/12/2026.
//

#pragma once

#include "Modal.hpp"

#include <Common/Platform.hpp>

#include <functional>

namespace Xen {
    /// Confirm deleting a file or folder. It goes to the Recycle Bin rather
    /// than being deleted for good - these are the user's project files, and
    /// nothing here tracks references to them.
    class DeleteModal final : public Modal {
    public:
        using Callback = std::function<void()>;

        /// @param OnDeleted called once the item has been moved to the Recycle Bin.
        void Open(const fs::path& Target, bool IsDirectory, Callback OnDeleted);

    protected:
        NODISCARD const char* Title() const override { return "Delete"; }
        void DrawBody() override;

    private:
        fs::path _Target;
        bool _IsDirectory {false};
        Callback _OnDeleted;
    };
}  // namespace Xen
