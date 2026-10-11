//
// Created by Jake Rieger on 10/3/2026.
//
// Host-side loader for a game module (see Module.hpp): loads the DLL, checks
// it was built against this exact engine and C++ runtime, creates/destroys its
// Game, and unloads it again - removing its component types from the registry
// first, since their factories are code inside the DLL.
//
// Unloading is only safe once nothing created from the module is left: no
// Game from CreateGame, no component instances (their vtables live in the
// DLL), no engine-held callbacks into it. The host's job is to tear all of
// that down first - in XED, by destroying the embedded Game, which owns the
// scene and every component in it.

#pragma once

#include <Common/XenCommon.hpp>

#include "ComponentRegistry.hpp"
#include "Module.hpp"

#include <string>

namespace Xen {
    class GameModule {
    public:
        GameModule() = default;
        ~GameModule();

        GameModule(const GameModule&)            = delete;
        GameModule& operator=(const GameModule&) = delete;

        /// @brief Loads the module at DllPath and validates it against this
        /// host. On failure returns false with Error describing why, and leaves
        /// nothing loaded or registered. Unloads any previously loaded module
        /// first.
        bool Load(const fs::path& DllPath, std::string& Error);

        /// @brief Unregisters the module's component types and unloads the DLL.
        /// Every Game and component created from it must already be destroyed.
        /// Safe to call when nothing is loaded.
        void Unload();

        NODISCARD bool IsLoaded() const { return _Handle != nullptr; }
        NODISCARD ModuleID GetID() const { return _ID; }
        NODISCARD const ModuleInfo& GetInfo() const { return _Info; }
        NODISCARD const fs::path& GetPath() const { return _Path; }

        /// @brief Constructs the module's Game subclass in embedded mode. Pair
        /// every call with DestroyGame, before Unload.
        NODISCARD Game* CreateGame(const EmbeddedGameArgs& Args) const;
        void DestroyGame(Game* G) const;

    private:
        void* _Handle {nullptr};  // HMODULE
        ModuleID _ID {NoModule};
        ModuleInfo _Info {};
        fs::path _Path;
        ModuleCreateGameFn _CreateGame {nullptr};
        ModuleDestroyGameFn _DestroyGame {nullptr};
    };
}  // namespace Xen
