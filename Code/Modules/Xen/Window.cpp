//
// Created by Jake Rieger on 9/8/2026.
//

#include <Common/Log.hpp>
#include <Common/Exception.hpp>

#include "Window.hpp"

namespace Xen {
    namespace {
        constexpr wchar_t WINDOW_CLASS_NAME[] = L"XenWindowClass";
        int g_WindowCount                     = 0;

        std::wstring Utf8ToWide(const std::string& Str) {
            if (Str.empty()) return {};
            const int Needed =
              MultiByteToWideChar(CP_UTF8, 0, Str.data(), CAST<int>(Str.size()), nullptr, 0);
            std::wstring Out(CAST<size_t>(Needed), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, Str.data(), CAST<int>(Str.size()), Out.data(), Needed);
            return Out;
        }
    }  // namespace

    Window::Window(const std::string& Title, const EngineConfig::WindowMode Mode, const u32 Width, const u32 Height) {
        const HINSTANCE Instance = GetModuleHandleW(nullptr);

        if (g_WindowCount == 0) {
            WNDCLASSEXW WndClass {};
            WndClass.cbSize        = sizeof(WNDCLASSEXW);
            WndClass.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
            WndClass.lpfnWndProc   = &Window::WndProc;
            WndClass.hInstance     = Instance;
            WndClass.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
            WndClass.lpszClassName = WINDOW_CLASS_NAME;

            if (!RegisterClassExW(&WndClass)) {
                THROW_ENGINE_EXCEPTION(EngineException, "RegisterClassExW failed");
            }
        }

        DWORD Style       = WS_OVERLAPPEDWINDOW;
        constexpr DWORD ExStyle = 0;
        u32 CreateWidth   = Width;
        u32 CreateHeight  = Height;

        if (Mode != EngineConfig::WindowMode::Windowed) {
            // Borderless and (borderless-)fullscreen both cover the whole monitor
            // with no decoration - true exclusive-fullscreen mode switching is a
            // swap chain concern, not a window one, and is out of scope here.
            Style = WS_POPUP;

            const HMONITOR Monitor = MonitorFromPoint(POINT {0, 0}, MONITOR_DEFAULTTOPRIMARY);
            MONITORINFO MonInfo {};
            MonInfo.cbSize = sizeof(MONITORINFO);
            GetMonitorInfoW(Monitor, &MonInfo);

            CreateWidth  = CAST<u32>(MonInfo.rcMonitor.right - MonInfo.rcMonitor.left);
            CreateHeight = CAST<u32>(MonInfo.rcMonitor.bottom - MonInfo.rcMonitor.top);
        }

        RECT WindowRect {0, 0, CAST<LONG>(CreateWidth), CAST<LONG>(CreateHeight)};
        AdjustWindowRectEx(&WindowRect, Style, FALSE, ExStyle);

        const std::wstring WideTitle = Utf8ToWide(Title);

        _Handle = CreateWindowExW(ExStyle,
                                  WINDOW_CLASS_NAME,
                                  WideTitle.c_str(),
                                  Style,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  WindowRect.right - WindowRect.left,
                                  WindowRect.bottom - WindowRect.top,
                                  nullptr,
                                  nullptr,
                                  Instance,
                                  this);

        if (!_Handle) {
            const DWORD LastError = GetLastError();
            if (g_WindowCount == 0) UnregisterClassW(WINDOW_CLASS_NAME, Instance);
            THROW_ENGINE_EXCEPTION(EngineException,
                                   "CreateWindowExW failed (GetLastError=" + std::to_string(LastError) + ")");
        }
        ++g_WindowCount;

        _Width  = CreateWidth;
        _Height = CreateHeight;

        if (Mode == EngineConfig::WindowMode::Windowed) CenterWindowOnScreen();

        ShowWindow(_Handle, SW_SHOW);
        UpdateWindow(_Handle);

        try {
            _InputManager.LoadInputMap("Config/InputConfig.ini");
        } catch (const EngineException& Ex) {
            LOG_WARN("failed to load InputConfig.ini, game will not be able to use action mappings: %s", Ex.what());
        }
    }

    Window::~Window() {
        Shutdown();
    }

    void Window::PollEvents() const {
        MSG Msg;
        while (PeekMessageW(&Msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&Msg);
            DispatchMessageW(&Msg);
        }
    }

    bool Window::ConsumeResized() {
        const bool Was = _Resized;
        _Resized       = false;
        return Was;
    }

    void Window::ResetInput() {
        _InputManager.ResetMouseDeltas();
    }

    LRESULT CALLBACK Window::WndProc(const HWND Handle, const UINT Msg, const WPARAM WParam, const LPARAM LParam) {
        Window* Self;
        if (Msg == WM_NCCREATE) {
            const auto* Create = RCAST<const CREATESTRUCTW*>(LParam);
            Self                = CAST<Window*>(Create->lpCreateParams);
            SetWindowLongPtrW(Handle, GWLP_USERDATA, RCAST<LONG_PTR>(Self));
        } else {
            Self = RCAST<Window*>(GetWindowLongPtrW(Handle, GWLP_USERDATA));
        }

        if (Self) return Self->HandleMessage(Handle, Msg, WParam, LParam);
        return DefWindowProcW(Handle, Msg, WParam, LParam);
    }

    LRESULT Window::HandleMessage(const HWND Handle, const UINT Msg, const WPARAM WParam, const LPARAM LParam) {
        switch (Msg) {
            case WM_CLOSE:
                // Deliberately not calling DestroyWindow/DefWindowProc: the game
                // loop polls ShouldClose() and tears the window down itself once
                // it notices, mirroring glfwWindowShouldClose's semantics.
                _ShouldClose = true;
                return 0;

            case WM_SIZE: {
                const u32 W = CAST<u32>(LOWORD(LParam));
                const u32 H = CAST<u32>(HIWORD(LParam));
                if (W != _Width || H != _Height) {
                    _Width   = W;
                    _Height  = H;
                    _Resized = true;
                }
                return 0;
            }

            // Hide the cursor only while it's over the client area, matching
            // GLFW_CURSOR_HIDDEN (still moves, just invisible - never captured).
            case WM_SETCURSOR:
                if (LOWORD(LParam) == HTCLIENT) {
                    SetCursor(nullptr);
                    return TRUE;
                }
                return DefWindowProcW(Handle, Msg, WParam, LParam);

            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYUP: {
                const bool Pressed = Msg == WM_KEYDOWN || Msg == WM_SYSKEYDOWN;
                _InputManager.UpdateKeyState(TranslateVirtualKey(WParam, LParam), Pressed);
                return 0;
            }

            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
                _InputManager.UpdateMouseButtonState(Input::MouseButton::Left, Msg == WM_LBUTTONDOWN);
                return 0;

            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
                _InputManager.UpdateMouseButtonState(Input::MouseButton::Right, Msg == WM_RBUTTONDOWN);
                return 0;

            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
                _InputManager.UpdateMouseButtonState(Input::MouseButton::Middle, Msg == WM_MBUTTONDOWN);
                return 0;

            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP: {
                const i16 Button =
                  HIWORD(WParam) == XBUTTON1 ? Input::MouseButton::Button4 : Input::MouseButton::Button5;
                _InputManager.UpdateMouseButtonState(Button, Msg == WM_XBUTTONDOWN);
                return TRUE;
            }

            case WM_MOUSEMOVE: {
                // GLFW's cursor-pos callback reports an absolute position and
                // InputManager derives its own delta; do the same here rather
                // than trusting raw input deltas, so both backends feed it
                // identically.
                static i32 LastX = 0, LastY = 0;
                const auto X = CAST<i32>(CAST<short>(LOWORD(LParam)));
                const auto Y = CAST<i32>(CAST<short>(HIWORD(LParam)));
                _InputManager.UpdateMousePosition(X - LastX, Y - LastY);
                LastX = X;
                LastY = Y;
                return 0;
            }

            default:
                return DefWindowProcW(Handle, Msg, WParam, LParam);
        }
    }

    i16 Window::TranslateVirtualKey(const WPARAM WParam, const LPARAM LParam) {
        i16 Key = CAST<i16>(WParam);

        if (Key == VK_SHIFT || Key == VK_CONTROL || Key == VK_MENU) {
            const auto ScanCode    = CAST<UINT>((LParam >> 16) & 0xFF);
            const UINT Mapped = MapVirtualKeyW(ScanCode, MAPVK_VSC_TO_VK_EX);
            if (Mapped != 0) Key = CAST<i16>(Mapped);
        }

        return Key;
    }

    void Window::Shutdown() {
        if (_Handle) {
            DestroyWindow(_Handle);
            _Handle = nullptr;
            --g_WindowCount;
        }

        if (g_WindowCount == 0) UnregisterClassW(WINDOW_CLASS_NAME, GetModuleHandleW(nullptr));
    }

    void Window::CenterWindowOnScreen() const {
        RECT WindowRect;
        GetWindowRect(_Handle, &WindowRect);
        const int WinW = WindowRect.right - WindowRect.left;
        const int WinH = WindowRect.bottom - WindowRect.top;

        const HMONITOR Monitor = MonitorFromWindow(_Handle, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO MonInfo {};
        MonInfo.cbSize = sizeof(MONITORINFO);
        GetMonitorInfoW(Monitor, &MonInfo);

        const int ScreenX = MonInfo.rcWork.left + ((MonInfo.rcWork.right - MonInfo.rcWork.left) - WinW) / 2;
        const int ScreenY = MonInfo.rcWork.top + ((MonInfo.rcWork.bottom - MonInfo.rcWork.top) - WinH) / 2;

        SetWindowPos(_Handle, nullptr, ScreenX, ScreenY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
}  // namespace Xen
