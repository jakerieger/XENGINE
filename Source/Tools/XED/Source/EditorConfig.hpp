//
// Created by Jake Rieger on 9/28/2026.
//

#pragma once

#include <filesystem>

namespace Xen {
    struct EditorConfig {
        std::filesystem::path CurrentProject;
    };
}  // namespace Xen