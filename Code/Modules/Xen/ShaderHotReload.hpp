//
// Created by Jake Rieger on 9/23/2026.
//
// Watches Code/Shaders/*.hlsl (and Include/*.hlsli) for edits while the game
// is running, and recompiles a changed shader through dxc.exe - the same
// command line Scripts/compile_engine_shaders.py already runs at build time
// - into EngineContent/Shaders, the exact directory the offline build
// already writes into and PAKTool already packs from. A PAK::LooseFileSource
// mounted over that same directory (see Game::Game) picks up the fresh DXIL
// bytes on the very next Assets.Load() - no pak repack, no asset-registry
// restart, since a LooseFileSource re-reads its file fresh on every call
// (see PAK::LooseFileSource::LoadFull).
//
// This only gets a shader's own bytes reloaded, not the pipeline built from
// them - Game::Poll's caller (Game::TickFrame) still has to tear down and
// rebuild whatever subsystems own a pipeline (MeshRenderer, FXAA, ...) when
// Poll() reports a change, the same Shutdown()/Initialize() pair each
// already exposes. Coarse (every shader-owning subsystem reloads, not just
// the one pipeline that actually changed), but simple and safe: destroying
// and recreating a PipelineHandle/ShaderHandle needs no IRenderDevice::
// WaitIdle (see D3D12RenderDevice's deferred-delete queue, the same
// mechanism Viewport::Resize already relies on for textures), so there's no
// GPU synchronization to get right, just an ordinary single-threaded
// Shutdown()-then-Initialize() call.

#pragma once

#include <Common/XenCommon.hpp>

#include <filesystem>
#include <memory>

// Same convention as DebugUI.hpp's XEN_WITH_DEBUG_UI - on by default in a
// debug build, off in release. Override by defining this before including
// the header (e.g. a CMake target_compile_definitions) if a build config
// wants this stripped from an otherwise non-NDEBUG build.
#ifndef XEN_WITH_SHADER_HOT_RELOAD
    #ifdef NDEBUG
        #define XEN_WITH_SHADER_HOT_RELOAD 0
    #else
        #define XEN_WITH_SHADER_HOT_RELOAD 1
    #endif
#endif

namespace Xen {
    /// @brief Every method is a safe no-op when XEN_WITH_SHADER_HOT_RELOAD is
    /// 0 (a Release build), or when Initialize's directories are empty
    /// (AssetSettings::EngineShaderSourceDir/OutputDir are only ever
    /// populated in a Debug build's generated settings - see
    /// XenGameSettings.h.in) or don't exist on disk (a build whose exe was
    /// copied away from the engine's own source tree) - so a call site never
    /// needs its own #if, same convention as DebugUI.
    class ShaderHotReload {
    public:
        ShaderHotReload();
        ~ShaderHotReload();

        ShaderHotReload(const ShaderHotReload&)            = delete;
        ShaderHotReload& operator=(const ShaderHotReload&) = delete;

        /// @brief SourceDir is watched for .hlsl/.hlsli edits; a changed
        /// file is recompiled into OutputDir via DxcPath (an absolute path -
        /// dxc.exe is almost never on this process's own PATH, only on the
        /// VS dev-tools one the build itself ran under; see XenGame.cmake).
        /// Records every current file's mtime as the baseline - nothing is
        /// recompiled just from calling this (the offline build already
        /// compiled whatever's on disk right now). Returns false
        /// (permanently inert) if SourceDir/OutputDir are empty or don't
        /// exist; DxcPath empty instead falls back to a bare "dxc.exe" PATH
        /// lookup (logged once, since it will likely fail).
        bool Initialize(const std::filesystem::path& SourceDir,
                        const std::filesystem::path& OutputDir,
                        const std::filesystem::path& DxcPath);

        /// @brief Call once per frame, outside IRenderDevice::BeginFrame/
        /// EndFrame - a caller that reloads shader-owning subsystems in
        /// response needs to do that outside an open frame too (the same
        /// constraint MeshRenderer::Initialize's synchronous BRDF LUT bake
        /// already has). Internally throttled to check the filesystem a few
        /// times a second, not every frame, so it's cheap to call
        /// unconditionally. Returns true if at least one shader was actually
        /// recompiled this call - the caller should then reload every
        /// subsystem that owns a pipeline built from Code/Shaders.
        NODISCARD bool Poll(f32 DeltaTime);

    private:
        struct Impl;
        std::unique_ptr<Impl> _Impl;
    };
}  // namespace Xen
