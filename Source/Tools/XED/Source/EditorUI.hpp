//
// Created by Jake Rieger on 9/28/2026.
//
// Dear ImGui plumbing for the editor itself - window/device setup, per-frame
// New/Render bracketing, Win32 message forwarding, and the SRV-heap bridge
// that lets the editor sample an embedded Game's Viewport into an
// ImGui::Image (GetOrCreateSceneTextureID). Deliberately NOT Xen::DebugUI:
// that class is the engine's own Debug/Release-gated overlay for a
// standalone game and is compiled out of a shippable build entirely -
// exactly what an editor cannot tolerate, since it needs its UI in every
// configuration, not just Debug. This class carries no XEN_WITH_DEBUG_UI (or
// any other) compile-time gate and lives in the editor's own module, never
// the shared Xen engine library, so nothing about how it's built is coupled
// to how a game's own debug overlay is built.
//
// Same "Initialize/BeginFrame/EndFrame bracket a frame" shape as DebugUI on
// purpose - not shared code, just the same proven shape - so anyone familiar
// with one recognizes the other immediately.

#pragma once

#include <Common/XenCommon.hpp>
#include <Xen/RenderDevice.hpp>
#include <Xen/UIOverlay.hpp>

#include <Windows.h>
#include <memory>

// ImTextureID's actual definition (imgui.h) - kept out of the rest of this
// header so a caller that never touches GetOrCreateSceneTextureID doesn't
// need Dear ImGui's own headers just to include EditorUI.hpp.
using ImTextureID = unsigned long long;

struct ImFont;

namespace Xen {
    class Window;

    class EditorUI final : public IUIOverlay {
    public:
        EditorUI();
        ~EditorUI() override;

        EditorUI(const EditorUI&)            = delete;
        EditorUI& operator=(const EditorUI&) = delete;

        /// @brief Device must be the engine's D3D12 backend and AppWindow
        /// must already have a live HWND.
        bool Initialize(RHI::IRenderDevice& Device, const Window& AppWindow);
        void Shutdown();

        NODISCARD bool IsInitialized() const { return _Initialized; }

        /// @brief Call once per frame, after IRenderDevice::BeginFrame - Dear
        /// ImGui calls (ImGui::Begin, etc.) are only valid between this and
        /// EndFrame.
        void BeginFrame();

        /// @brief Records Dear ImGui's draw data as an overlay on top of
        /// whatever's already in the current frame's swap chain (the
        /// editor's own base-layer clear), then restores the back buffer to
        /// its resting state. Call before IRenderDevice::EndFrame.
        void EndFrame();

        /// @brief Forwards a Win32 message to Dear ImGui; returns true if
        /// Dear ImGui consumed it. See Window::HandleMessage for the one
        /// message (WM_SETCURSOR) this alone isn't sufficient for.
        bool ProcessMessage(HWND Handle, UINT Msg, WPARAM WParam, LPARAM LParam) override;

        /// @brief True while the mouse is over/interacting with a Dear ImGui
        /// window - Window uses this to withhold raw-input mouse events (and
        /// decide who owns the cursor) from the game so e.g. dragging an
        /// editor window doesn't also spin the embedded game's camera.
        NODISCARD bool WantsCaptureMouse() const override;

        /// @brief Same as WantsCaptureMouse but for keyboard input.
        NODISCARD bool WantsCaptureKeyboard() const override;

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

        /// @brief Allocates a NEW, permanent SRV slot for Handle and returns
        /// its ImTextureID - unlike GetOrCreateSceneTextureID, which is
        /// deliberately one slot rewritten every call, this hands out a
        /// fresh slot each time and never reuses or rewrites it. For a
        /// texture that lives for its own lifetime and needs a stable
        /// ImTextureID (e.g. a toolbar icon, loaded once at startup) rather
        /// than a single "whatever's showing this frame" slot. Returns 0
        /// (ImTextureID_Invalid) if the SRV heap is full or Handle isn't a
        /// plain color TextureHandle.
        NODISCARD ImTextureID CreateStaticTextureID(RHI::TextureHandle Handle);

        bool LoadFont(const std::string& Name, const unsigned char* Data, u32 DataSize, f32 Pixels = 16.f) const;
        ImFont* GetFont(const std::string& Name) const;

    private:
        struct Impl;
        std::unique_ptr<Impl> _Impl;
        bool _Initialized {false};
    };

    struct ScopedFont {
        ScopedFont(const EditorUI* UI, const std::string& Name);
        ~ScopedFont();
    };
}  // namespace Xen
