//
// Created by Jake Rieger on 10/12/2026.
//

#pragma once

#include "Modal.hpp"

#include <Common/Platform.hpp>

#include <functional>
#include <string>

namespace Xen {
    /// Name and create a new folder inside another.
    class NewFolderModal final : public Modal {
    public:
        using Callback = std::function<void(const fs::path& NewFolder)>;

        /// @param Parent where the folder goes. The name starts as the first
        /// free "New Folder", "New Folder 2", ...
        /// @param OnCreated called with the new folder's path.
        void Open(const fs::path& Parent, Callback OnCreated);

    protected:
        NODISCARD const char* Title() const override { return "New Folder"; }
        void DrawBody() override;

    private:
        fs::path _Parent;
        Callback _OnCreated;
        char _Buffer[256] {};
        std::string _Error;
    };
}  // namespace Xen
