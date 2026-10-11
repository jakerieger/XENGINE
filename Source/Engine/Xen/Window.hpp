//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include "Input.hpp"
#include "EngineConfig.hpp"

#include <string>

namespace Xen {
    class IUIOverlay;

    class Window {
    public:
        Window(const std::string& Title, EngineConfig::WindowMode Mode, u32 Width, u32 Height);
        ~Window();

        Window(const Window&)            = delete;
        Window& operator=(const Window&) = delete;

        void PollEvents() const;

        /// @brief Optional - when set, Win32 messages are forwarded to UI
        /// before the engine's own handling (see HandleMessage), and raw
        /// keyboard/mouse input is withheld from InputManager while UI
        /// reports it wants that input (WantsCaptureMouse/Keyboard), so
        /// e.g. dragging a UI window doesn't also spin the game camera. UI
        /// is whatever implements IUIOverlay - DebugUI (a standalone game's
        /// own Debug/Release-gated overlay) or the editor's own always-on
        /// UI class; Window doesn't know or care which. Pass nullptr to
        /// detach. Not owned.
        void SetUIOverlay(IUIOverlay* UI) { _UIOverlay = UI; }

        NODISCARD bool ShouldClose() const { return _ShouldClose; }

        NODISCARD u32 GetWidth() const { return _Width; }
        NODISCARD u32 GetHeight() const { return _Height; }
        NODISCARD bool IsMinimized() const { return _Width == 0 || _Height == 0; }
        NODISCARD HWND GetHandle() const { return _Handle; }
        NODISCARD InputManager& GetInputManager() { return _InputManager; }

        NODISCARD bool ConsumeResized();

        void ResetInput();

        /// @brief Drops all current key/button state (see InputManager::Clear).
        void ClearInput();

        NODISCARD bool IsFocused() const { return _Focused; }

        /// @brief Locks the cursor to the middle of the window and hides it,
        /// for mouse-look: the cursor can't wander out of the window or onto
        /// UI, while mouse movement keeps arriving as deltas (raw input, so
        /// it is unaffected by the pinned cursor). While locked, UI doesn't
        /// get the mouse. The lock is released while the window is not
        /// focused and re-applied when focus returns.
        void SetCursorLocked(bool Locked);
        NODISCARD bool IsCursorLocked() const { return _CursorLocked; }

        void Maximize() const;
        void SetTitle(const std::string& Title) const;

    private:
        static LRESULT CALLBACK WndProc(HWND Handle, UINT Msg, WPARAM WParam, LPARAM LParam);

        /// @brief Handle is passed explicitly rather than read from _Handle: the
        /// earliest messages (WM_NCCREATE, WM_CREATE) arrive synchronously inside
        /// CreateWindowExW, before it has returned a value for _Handle to hold.
        LRESULT HandleMessage(HWND Handle, UINT Msg, WPARAM WParam, LPARAM LParam);

        /// @brief Registers this window for raw keyboard + mouse input (WM_INPUT).
        /// Raw input is the authoritative source for key/button state and mouse
        /// deltas - see HandleRawKeyboard/HandleRawMouse - because it reports every
        /// physical event straight from the driver: mouse deltas aren't run through
        /// OS pointer acceleration or clamped at the screen edge the way
        /// WM_MOUSEMOVE-derived deltas are, which matters for camera-look controls.
        /// WM_MOUSEMOVE is still used for absolute cursor position, which raw
        /// input's relative-motion stream doesn't provide.
        void RegisterRawInput() const;

        void HandleRawInput(LPARAM LParam);
        void HandleRawKeyboard(const RAWKEYBOARD& KB);
        void HandleRawMouse(const RAWMOUSE& Mouse);

        /// @brief Resolves a scan code to a distinct Left/Right code for
        /// Shift/Control/Alt - both raw input and the legacy messages report the
        /// generic VK_SHIFT/VK_CONTROL/VK_MENU for both keyboard halves at the
        /// virtual-key level; only the scan code tells them apart.
        static i16 DisambiguateModifierKey(i16 VKey, UINT ScanCode);

        /// @brief Pins the cursor to the client area's center if the lock is on
        /// and the window is focused; releases it otherwise.
        void ApplyCursorClip() const;

        void Shutdown();

        /// @brief Center the window on the active monitor.
        void CenterWindowOnScreen() const;

        HWND _Handle {nullptr};
        u32 _Width {0};
        u32 _Height {0};
        bool _Resized {false};
        bool _ShouldClose {false};
        bool _CursorLocked {false};
        bool _Focused {true};
        InputManager _InputManager;
        IUIOverlay* _UIOverlay {nullptr};
    };
}  // namespace Xen
