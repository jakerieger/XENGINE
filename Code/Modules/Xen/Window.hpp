//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once

#include <Common/XenCommon.hpp>
#include "Input.hpp"
#include "EngineConfig.hpp"

#include <string>

namespace Xen {
    class Window {
    public:
        Window(const std::string& Title, EngineConfig::WindowMode Mode, u32 Width, u32 Height);
        ~Window();

        Window(const Window&)            = delete;
        Window& operator=(const Window&) = delete;

        void PollEvents() const;

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

        /// @brief Resolves WM_KEYDOWN/UP's wParam to a distinct Left/Right code for
        /// Shift/Control/Alt - Win32 reports the generic VK_SHIFT/VK_CONTROL/VK_MENU
        /// for both keyboard halves at the wParam level; only the scan code (in
        /// lParam) tells them apart.
        static i16 TranslateVirtualKey(WPARAM WParam, LPARAM LParam);

        void Shutdown();

        /// @brief Center the window on the active monitor.
        void CenterWindowOnScreen() const;

        HWND _Handle {nullptr};
        u32 _Width {0};
        u32 _Height {0};
        bool _Resized {false};
        bool _ShouldClose {false};
        InputManager _InputManager;
    };
}  // namespace Xen
