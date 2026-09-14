//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once

namespace Xen {
    namespace PAK {
        class AssetRegistry;
    }
    class TextureCache;
    class Game;

    /// @brief Engine-wide services a scene and its components need.
    ///
    /// Passed to Scene at construction rather than reached for through a
    /// global, so tests can stand up a scene with fake services and two scenes
    /// can in principle use different ones.
    struct EngineContext {
        PAK::AssetRegistry* Assets {nullptr};
        TextureCache* Textures {nullptr};
        Game* Owner {nullptr};
    };
}  // namespace Xen