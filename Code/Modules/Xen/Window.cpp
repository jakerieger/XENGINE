//
// Created by Jake Rieger on 9/8/2026.
//

#include <Common/Log.hpp>
#include <Common/Exception.hpp>

#include <glad.h>
#include "Window.hpp"

#include <format>

namespace Xen {
    namespace {
        int g_WindowCount = 0;
    }

    Window::Window(const std::string& Title, EngineConfig::WindowMode Mode, const u32 Width, const u32 Height) {
        if (g_WindowCount == 0 && glfwInit() != GLFW_TRUE) {
            THROW_ENGINE_EXCEPTION(EngineException, "glfwInit failed");
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);  // Required for macOS
#endif

        if (Mode == EngineConfig::WindowMode::Windowed) {
            _Handle = glfwCreateWindow(CAST<int>(Width), CAST<int>(Height), Title.c_str(), nullptr, nullptr);
            if (!_Handle) { THROW_ENGINE_EXCEPTION(EngineException, "glfwCreateWindow failed"); }

            CenterWindowOnScreen();
        } else {
            if (Mode == EngineConfig::WindowMode::Borderless) { glfwWindowHint(GLFW_DECORATED, GL_FALSE); }
            GLFWmonitor* Monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* M = glfwGetVideoMode(Monitor);
            _Handle              = glfwCreateWindow(M->width,
                                       M->height,
                                       Title.c_str(),
                                       Mode == EngineConfig::WindowMode::Borderless ? nullptr : Monitor,
                                       nullptr);
            if (!_Handle) { THROW_ENGINE_EXCEPTION(EngineException, "glfwCreateWindow failed"); }
        }

        if (!_Handle) {
            if (g_WindowCount == 0) glfwTerminate();
            THROW_ENGINE_EXCEPTION(EngineException, "glfwCreateWindow failed");
        }
        ++g_WindowCount;

        glfwMakeContextCurrent(_Handle);

        if (!gladLoadGLLoader(RCAST<GLADloadproc>(glfwGetProcAddress))) {
            THROW_ENGINE_EXCEPTION(EngineException, "gladLoadGLLoader failed");
        }

        glfwSetWindowUserPointer(_Handle, this);
        glfwSetFramebufferSizeCallback(_Handle, &Window::OnFrameBufferResized);
        glfwSetKeyCallback(_Handle, &Window::OnKeyCallback);
        glfwSetMouseButtonCallback(_Handle, &Window::OnMouseButtonCallback);
        glfwSetCursorPosCallback(_Handle, &Window::OnCursorPosCallback);
        glfwSetScrollCallback(_Handle, &Window::OnMouseScrollCallback);

        // Framebuffer size, not window size: they differ on high-DPI displays
        // and the swap chain is sized in pixels.
        int W = 0, H = 0;
        glfwGetFramebufferSize(_Handle, &W, &H);
        _Width  = CAST<u32>(W);
        _Height = CAST<u32>(H);

        // Hide mouse cursor (this doesn't disable it)
        glfwSetInputMode(_Handle, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

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
        glfwPollEvents();
    }

    void Window::SwapBuffers() const {
        glfwSwapBuffers(_Handle);
    }

    void Window::Clear(const bool Depth) {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        auto ClearFlags = GL_COLOR_BUFFER_BIT;
        if (Depth) ClearFlags |= GL_DEPTH_BUFFER_BIT;
        glClear(ClearFlags);
    }

    bool Window::ShouldClose() const {
        return glfwWindowShouldClose(_Handle) == GLFW_TRUE;
    }

    bool Window::ConsumeResized() {
        const bool Was = _Resized;
        _Resized       = false;
        return Was;
    }

    void Window::ResetInput() {
        _InputManager.ResetMouseDeltas();
    }

    void Window::OnFrameBufferResized(GLFWwindow* Handle, const int W, const int H) {
        auto* Self = CAST<Window*>(glfwGetWindowUserPointer(Handle));
        if (!Self) return;
        Self->_Width   = CAST<u32>(W);
        Self->_Height  = CAST<u32>(H);
        Self->_Resized = true;
    }

    void Window::OnKeyCallback(GLFWwindow* Handle, const int Key, int, const int Action, int) {
        auto* Self = CAST<Window*>(glfwGetWindowUserPointer(Handle));
        if (!Self) return;

        if (Action == GLFW_PRESS) {
            Self->GetInputManager().UpdateKeyState(Key, true);
        } else if (Action == GLFW_RELEASE) {
            Self->GetInputManager().UpdateKeyState(Key, false);
        }
    }

    void Window::OnMouseButtonCallback(GLFWwindow* Handle, const int Button, const int Action, int) {
        auto* Self = CAST<Window*>(glfwGetWindowUserPointer(Handle));
        if (!Self) return;

        if (Action == GLFW_PRESS) {
            Self->GetInputManager().UpdateMouseButtonState(Button, true);
        } else if (Action == GLFW_RELEASE) {
            Self->GetInputManager().UpdateMouseButtonState(Button, false);
        }
    }

    void Window::OnCursorPosCallback(GLFWwindow* Handle, const double X, const double Y) {
        auto* Self = CAST<Window*>(glfwGetWindowUserPointer(Handle));
        if (!Self) return;
        Self->GetInputManager().UpdateMousePosition(X, Y);
    }

    void Window::OnMouseScrollCallback(GLFWwindow* Handle, double DeltaX, double DeltaY) {
        // TODO: Implement OnMouseScrollCallback
        (void)Handle;
        (void)DeltaX;
        (void)DeltaY;
    }

    void Window::Shutdown() const {
        if (_Handle) {
            glfwDestroyWindow(_Handle);
            --g_WindowCount;
        }

        if (g_WindowCount == 0) glfwTerminate();
    }

    void Window::CenterWindowOnScreen() const {
        int MonX, MonY, MonW, MonH;
        glfwGetMonitorWorkarea(glfwGetPrimaryMonitor(), &MonX, &MonY, &MonW, &MonH);

        int WinW, WinH;
        glfwGetWindowSize(_Handle, &WinW, &WinH);

        const int ScreenX = MonX + (MonW - WinW) / 2;
        const int ScreenY = MonY + (MonH - WinH) / 2;

        glfwSetWindowPos(_Handle, ScreenX, ScreenY);
    }
}  // namespace Xen