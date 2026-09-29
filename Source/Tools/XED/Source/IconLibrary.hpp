//
// Created by Jake Rieger on 9/29/2026.
//
// Turns the Brotli-compressed, embedded icon images in Resource/EditorIcons.h
// into GPU textures with stable ImTextureIDs, for toolbar/menu buttons.
// Decompression + upload happens once, in Initialize - after that, Get() is
// just an array lookup.

#pragma once

#include <Common/XenCommon.hpp>
#include <Xen/RenderDevice.hpp>

#include <array>

// Kept out of the rest of this header for the same reason as EditorUI.hpp's
// own copy - a caller that never touches Get() shouldn't need Dear ImGui's
// headers just to include this one.
using ImTextureID = unsigned long long;

namespace Xen {
    class EditorUI;

    enum class EditorIcon : u8 {
        CleanCode,
        CompileCode,
        FocusSelected,
        GridToggle,
        Move,
        OpenFolder,
        Pause,
        Play,
        PlayWindowed,
        Redo,
        Rotate,
        Scale,
        SelectAsset,
        Select,
        Stop,
        Undo,
        Count
    };

    class IconLibrary {
    public:
        ~IconLibrary() { Shutdown(); }

        /// @brief Decompresses and uploads every icon in Resource/EditorIcons.h
        /// as its own GPU texture, and registers each with UI (via
        /// EditorUI::CreateStaticTextureID) for a stable ImTextureID. Device
        /// and UI must both already be initialized, and must outlive this
        /// IconLibrary. Returns false only if UI itself isn't initialized;
        /// an individual icon failing to decompress/upload is logged and
        /// skipped rather than failing the whole call, matching the rest of
        /// the codebase's "safe no-op instead of a crash" convention - Get()
        /// just returns 0 for that icon, and Dear ImGui simply has nothing
        /// to draw for a 0 (ImTextureID_Invalid) texture ID.
        bool Initialize(RHI::IRenderDevice& Device, EditorUI& UI);

        /// @brief Frees every icon's GPU texture. Safe to call even if
        /// Initialize was never called, or failed partway through.
        void Shutdown();

        /// @brief The icon's ImTextureID, or 0 if it failed to load - see
        /// Initialize's own comment on why a caller should treat that as
        /// "skip this button" rather than an error to handle.
        NODISCARD ImTextureID Get(EditorIcon Icon) const;

    private:
        RHI::IRenderDevice* _Device {nullptr};
        std::array<RHI::TextureHandle, CAST<size_t>(EditorIcon::Count)> _Textures {};
        std::array<ImTextureID, CAST<size_t>(EditorIcon::Count)> _TextureIDs {};
    };
}  // namespace Xen
