//
// Created by Jake Rieger on 9/18/2026.
//
// Dear ImGui plumbing: window/device setup, per-frame New/Render bracketing,
// and Win32 message forwarding - the same "Initialize/BeginFrame/EndFrame
// bracket a frame" shape as SpriteRenderer/MeshRenderer. No specific debug
// windows (frame stats, dev tools, a console, ...) are built into this class
// on purpose: once BeginFrame/EndFrame bracket a frame, any Dear ImGui call
// (ImGui::Begin, etc.) works from anywhere in between, most naturally from
// Game::OnRender - that's the actual extension mechanism, not something this
// class needs to wrap.

#pragma once

#include <Common/XenCommon.hpp>

#include "RenderDevice.hpp"

#include <Windows.h>
#include <memory>

// ImTextureID's actual definition (imgui.h) - kept out of the rest of this
// header so a caller that never touches GetOrCreateSceneTextureID doesn't
// need Dear ImGui's own headers just to include DebugUI.hpp.
using ImTextureID = unsigned long long;

// On by default in a debug build, off in release - matches the engine's
// existing NDEBUG-gated debug tooling (Game.hpp's console allocation,
// D3D12RenderDevice's validation layer). Override by defining this before
// including the header (e.g. a CMake target_compile_definitions) if a build
// config wants debug tooling in an otherwise-NDEBUG build, or wants it
// stripped from an otherwise non-NDEBUG one.
#ifndef XEN_WITH_DEBUG_UI
    #ifdef NDEBUG
        #define XEN_WITH_DEBUG_UI 0
    #else
        #define XEN_WITH_DEBUG_UI 1
    #endif
#endif

namespace Xen {
    class Window;

    /// @brief Every method is a safe no-op (and IsInitialized()/
    /// WantsCapture*() always false) when XEN_WITH_DEBUG_UI is 0, so call
    /// sites (Game::TickFrame, Window::HandleMessage) never need their own
    /// #if - in a release build this class doesn't even include Dear ImGui's
    /// headers, let alone create a context or do per-frame work.
    class DebugUI {
    public:
        DebugUI();
        ~DebugUI();

        DebugUI(const DebugUI&)            = delete;
        DebugUI& operator=(const DebugUI&) = delete;

        /// @brief Device must be the engine's D3D12 backend (the only one
        /// that exists - see D3D12RenderDevice.hpp's "Debug UI integration"
        /// section) and AppWindow must already have a live HWND.
        bool Initialize(RHI::IRenderDevice& Device, const Window& AppWindow);
        void Shutdown();

        NODISCARD bool IsInitialized() const { return _Initialized; }

        /// @brief Call once per frame, after IRenderDevice::BeginFrame - Dear
        /// ImGui calls (ImGui::Begin, etc.) are only valid between this and
        /// EndFrame.
        void BeginFrame();

        /// @brief Records Dear ImGui's draw data as an overlay on top of
        /// whatever's already in the current frame (Game's own rendering,
        /// CopyToSwapChain, ...), then restores the back buffer to its
        /// resting state. Call after all of that but before
        /// IRenderDevice::EndFrame.
        void EndFrame();

        /// @brief Forwards a Win32 message to Dear ImGui; returns true if
        /// Dear ImGui consumed it. See Window::HandleMessage for the one
        /// message (WM_SETCURSOR) this alone isn't sufficient for.
        bool ProcessMessage(HWND Handle, UINT Msg, WPARAM WParam, LPARAM LParam);

        /// @brief True while the mouse is over/interacting with a Dear ImGui
        /// window - Window uses this to withhold raw-input mouse events (and
        /// decide who owns the cursor) from the game so e.g. dragging a
        /// debug window doesn't also spin the game camera.
        NODISCARD bool WantsCaptureMouse() const;

        /// @brief Same as WantsCaptureMouse but for keyboard input (e.g.
        /// typing into a debug console shouldn't also move the player).
        NODISCARD bool WantsCaptureKeyboard() const;

        /// @brief An ImTextureID for Handle's current contents (e.g. an
        /// embedded Game's Viewport::GetColorTarget()), for
        /// ImGui::Image(...) - implicitly convertible to the newer
        /// ImTextureRef parameter type ImGui::Image itself now takes. Safe
        /// to call every frame: rewrites one dedicated descriptor slot each
        /// time rather than caching by handle, so a Viewport::Resize (which
        /// destroys and recreates its color target under a NEW handle) never
        /// leaves a stale cache entry to invalidate. One slot only - fine
        /// for one "Scene" panel; a second simultaneous viewport would need
        /// a small handle-keyed map at that point, not before. Returns 0
        /// (ImTextureID_Invalid) if not initialized or Handle isn't a plain
        /// color TextureHandle (see D3D12RenderDevice::CreateTextureSRV).
        NODISCARD ImTextureID GetOrCreateSceneTextureID(RHI::TextureHandle Handle);

        bool LoadFont(const std::string& Name, const unsigned char* Data, size_t DataSize, f32 Pixels = 16.f) const;

    private:
        struct Impl;
        std::unique_ptr<Impl> _Impl;
        bool _Initialized {false};
    };
}  // namespace Xen
