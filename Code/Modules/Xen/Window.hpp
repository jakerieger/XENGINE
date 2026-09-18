//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once

#include <Common/XenCommon.hpp>
#include "Input.hpp"
#include "EngineConfig.hpp"

#include <string>

namespace Xen {
    class DebugUI;

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
        /// e.g. dragging a debug window doesn't also spin the game camera.
        /// Pass nullptr to detach. Not owned.
        void SetDebugUI(DebugUI* UI) { _DebugUI = UI; }

        NODISCARD bool ShouldClose() const { return _ShouldClose; }

        NODISCARD u32 GetWidth() const { return _Width; }
        NODISCARD u32 GetHeight() const { return _Height; }
        NODISCARD bool IsMinimized() const { return _Width == 0 || _Height == 0; }
        NODISCARD HWND GetHandle() const { return _Handle; }
        NODISCARD InputManager& GetInputManager() { return _InputManager; }

        NODISCARD bool ConsumeResized();

        void ResetInput();

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

        void Shutdown();

        /// @brief Center the window on the active monitor.
        void CenterWindowOnScreen() const;

        HWND _Handle {nullptr};
        u32 _Width {0};
        u32 _Height {0};
        bool _Resized {false};
        bool _ShouldClose {false};
        InputManager _InputManager;
        DebugUI* _DebugUI {nullptr};
    };
}  // namespace Xen
