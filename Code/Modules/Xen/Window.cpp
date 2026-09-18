//
// Created by Jake Rieger on 9/8/2026.
//

#include <Common/Log.hpp>
#include <Common/Exception.hpp>

#include "Window.hpp"
#include "DebugUI.hpp"

#include <vector>

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

        RegisterRawInput();

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
        _InputManager.EndFrame();
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
        // Forwarded first (when DebugUI is active) for Dear ImGui's own
        // input/IME handling - except WM_SETCURSOR, whose "handled" return
        // value doesn't mean "the mouse is over a Dear ImGui window"
        // (ImGui_ImplWin32_UpdateMouseCursor sets a cursor unconditionally
        // whenever it isn't explicitly told not to), so that one is decided
        // below by WantsCaptureMouse() instead.
        if (_DebugUI && Msg != WM_SETCURSOR && _DebugUI->ProcessMessage(Handle, Msg, WParam, LParam)) {
            return TRUE;
        }

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
            // GLFW_CURSOR_HIDDEN (still moves, just invisible - never
            // captured) - unless Dear ImGui wants the mouse (hovering/
            // dragging a debug window), in which case let it manage the
            // cursor (resize arrows, text-input beam, ...) instead of
            // forcing it hidden, or every debug window becomes unusable blind.
            case WM_SETCURSOR:
                if (LOWORD(LParam) == HTCLIENT) {
                    if (_DebugUI && _DebugUI->WantsCaptureMouse()) {
                        _DebugUI->ProcessMessage(Handle, Msg, WParam, LParam);
                        return TRUE;
                    }
                    SetCursor(nullptr);
                    return TRUE;
                }
                return DefWindowProcW(Handle, Msg, WParam, LParam);

            // Key/button *state* comes entirely from WM_INPUT now (see
            // HandleRawKeyboard/HandleRawMouse) - it reports every physical
            // event straight from the driver rather than through OS pointer
            // acceleration/edge-clamping. WM_SYSKEYDOWN/UP (Alt, F10, Alt+F4,
            // ...) are deliberately forwarded to DefWindowProcW unconditionally
            // rather than handled here: that default processing is what
            // actually implements Alt+F4 closing the window, and swallowing
            // the message (as this used to do) silently broke it.
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
                return DefWindowProcW(Handle, Msg, WParam, LParam);

            case WM_INPUT:
                HandleRawInput(LParam);
                return 0;

            case WM_MOUSEMOVE: {
                // Absolute client-area position only - raw input's relative-motion
                // stream (used for deltas) doesn't carry this.
                const auto X = CAST<i32>(CAST<short>(LOWORD(LParam)));
                const auto Y = CAST<i32>(CAST<short>(HIWORD(LParam)));
                _InputManager.SetMousePosition(X, Y);
                return 0;
            }

            default:
                return DefWindowProcW(Handle, Msg, WParam, LParam);
        }
    }

    void Window::RegisterRawInput() const {
        RAWINPUTDEVICE Devices[2] {};

        // Mouse: usage page 1 ("generic desktop"), usage 2 ("mouse").
        Devices[0].usUsagePage = 0x01;
        Devices[0].usUsage     = 0x02;
        Devices[0].dwFlags     = 0;
        Devices[0].hwndTarget  = _Handle;

        // Keyboard: usage page 1, usage 6 ("keyboard"). No RIDEV_NOLEGACY - the
        // legacy WM_KEYDOWN/WM_SYSKEYDOWN stream keeps flowing too, which is what
        // lets WM_SYSKEYDOWN/UP still reach DefWindowProcW for system shortcuts
        // (Alt+F4, the system menu) while WM_INPUT is what actually drives game
        // input state.
        Devices[1].usUsagePage = 0x01;
        Devices[1].usUsage     = 0x06;
        Devices[1].dwFlags     = 0;
        Devices[1].hwndTarget  = _Handle;

        RegisterRawInputDevices(Devices, 2, sizeof(RAWINPUTDEVICE));
    }

    void Window::HandleRawInput(const LPARAM LParam) {
        UINT Size = 0;
        GetRawInputData(RCAST<HRAWINPUT>(LParam), RID_INPUT, nullptr, &Size, sizeof(RAWINPUTHEADER));
        if (Size == 0) return;

        // Mouse/keyboard RAWINPUT is well under this in practice; fall back to a
        // heap buffer only on the off chance a future device reports more.
        alignas(alignof(RAWINPUT)) BYTE StackBuffer[64];
        std::vector<BYTE> HeapBuffer;
        BYTE* Buffer = StackBuffer;
        if (Size > sizeof(StackBuffer)) {
            HeapBuffer.resize(Size);
            Buffer = HeapBuffer.data();
        }

        if (GetRawInputData(RCAST<HRAWINPUT>(LParam), RID_INPUT, Buffer, &Size, sizeof(RAWINPUTHEADER)) != Size) return;

        const auto* Raw = RCAST<const RAWINPUT*>(Buffer);
        if (Raw->header.dwType == RIM_TYPEKEYBOARD) {
            HandleRawKeyboard(Raw->data.keyboard);
        } else if (Raw->header.dwType == RIM_TYPEMOUSE) {
            HandleRawMouse(Raw->data.mouse);
        }
    }

    void Window::HandleRawKeyboard(const RAWKEYBOARD& KB) {
        // 0xFF is raw input's "this event carries no valid key" sentinel (e.g.
        // one half of an escaped multi-byte sequence) - not a real key press.
        if (KB.VKey == 0xFF) return;

        // Raw input bypasses Dear ImGui's own WM_KEYDOWN/WM_CHAR-based
        // keyboard handling entirely, so without this a debug console text
        // field and the game would both react to every keystroke.
        if (_DebugUI && _DebugUI->WantsCaptureKeyboard()) return;

        const bool Pressed = (KB.Flags & RI_KEY_BREAK) == 0;
        const i16 Key      = DisambiguateModifierKey(CAST<i16>(KB.VKey), KB.MakeCode);

        _InputManager.UpdateKeyState(Key, Pressed);
    }

    void Window::HandleRawMouse(const RAWMOUSE& Mouse) {
        // Same reasoning as HandleRawKeyboard: raw input bypasses Dear
        // ImGui's own mouse handling, so dragging a debug window would also
        // orbit the game camera without this.
        if (_DebugUI && _DebugUI->WantsCaptureMouse()) return;

        // Absolute-mode devices (pen tablets, some VM/RDP setups) aren't handled
        // here - they're rare enough for a desktop game not to special-case, and
        // WM_MOUSEMOVE already covers absolute position for the normal case.
        if ((Mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0 && (Mouse.lLastX != 0 || Mouse.lLastY != 0)) {
            _InputManager.AddMouseDelta(Mouse.lLastX, Mouse.lLastY);
        }

        struct ButtonMapping {
            USHORT DownFlag;
            USHORT UpFlag;
            i16 Button;
        };
        static constexpr ButtonMapping Mappings[] = {
          {RI_MOUSE_LEFT_BUTTON_DOWN,   RI_MOUSE_LEFT_BUTTON_UP,   Input::MouseButton::Left  },
          {RI_MOUSE_RIGHT_BUTTON_DOWN,  RI_MOUSE_RIGHT_BUTTON_UP,  Input::MouseButton::Right },
          {RI_MOUSE_MIDDLE_BUTTON_DOWN, RI_MOUSE_MIDDLE_BUTTON_UP, Input::MouseButton::Middle},
          {RI_MOUSE_BUTTON_4_DOWN,      RI_MOUSE_BUTTON_4_UP,      Input::MouseButton::Button4},
          {RI_MOUSE_BUTTON_5_DOWN,      RI_MOUSE_BUTTON_5_UP,      Input::MouseButton::Button5},
        };

        for (const auto& [DownFlag, UpFlag, Button] : Mappings) {
            if (Mouse.usButtonFlags & DownFlag) _InputManager.UpdateMouseButtonState(Button, true);
            if (Mouse.usButtonFlags & UpFlag) _InputManager.UpdateMouseButtonState(Button, false);
        }
    }

    i16 Window::DisambiguateModifierKey(const i16 VKey, const UINT ScanCode) {
        if (VKey != VK_SHIFT && VKey != VK_CONTROL && VKey != VK_MENU) return VKey;

        const UINT Mapped = MapVirtualKeyW(ScanCode, MAPVK_VSC_TO_VK_EX);
        return Mapped != 0 ? CAST<i16>(Mapped) : VKey;
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
