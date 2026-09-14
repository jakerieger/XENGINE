//
// Created by Jake Rieger on 9/8/2026.
//

#include "Window.hpp"

#define GLFW_INCLUDE_NONE
#include "Exception.hpp"

#include <glad.h>
#include <GLFW/glfw3.h>

#include <format>

namespace Xen {
    namespace {
        int g_WindowCount = 0;
    }

    Window::Window(const std::string& Title, Mode WindowMode, const u32 Width, const u32 Height) {
        if (g_WindowCount == 0 && glfwInit() != GLFW_TRUE) {
            _ThrowEngineException(EngineException, "glfwInit failed");
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);  // Required for macOS
#endif

        if (WindowMode == Mode::Windowed) {
            _Handle = glfwCreateWindow(CAST<int>(Width), CAST<int>(Height), Title.c_str(), nullptr, nullptr);
            if (!_Handle) { _ThrowEngineException(EngineException, "glfwCreateWindow failed"); }

            CenterWindowOnScreen();
        } else {
            if (WindowMode == Mode::Borderless) { glfwWindowHint(GLFW_DECORATED, GL_FALSE); }
            GLFWmonitor* Monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* M = glfwGetVideoMode(Monitor);
            _Handle              = glfwCreateWindow(M->width,
                                       M->height,
                                       Title.c_str(),
                                       WindowMode == Mode::Borderless ? nullptr : Monitor,
                                       nullptr);
            if (!_Handle) { _ThrowEngineException(EngineException, "glfwCreateWindow failed"); }
        }

        if (!_Handle) {
            if (g_WindowCount == 0) glfwTerminate();
            _ThrowEngineException(EngineException, "glfwCreateWindow failed");
        }
        ++g_WindowCount;

        glfwMakeContextCurrent(_Handle);

        if (!gladLoadGLLoader(RCAST<GLADloadproc>(glfwGetProcAddress))) {
            _ThrowEngineException(EngineException, "gladLoadGLLoader failed");
        }

        glfwSetWindowUserPointer(_Handle, this);
        glfwSetFramebufferSizeCallback(_Handle, &Window::OnFramebufferResized);

        // Framebuffer size, not window size: they differ on high-DPI displays
        // and the swap chain is sized in pixels.
        int W = 0, H = 0;
        glfwGetFramebufferSize(_Handle, &W, &H);
        _Width  = CAST<u32>(W);
        _Height = CAST<u32>(H);
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

    void Window::OnFramebufferResized(GLFWwindow* Handle, const int W, const int H) {
        auto* Self = CAST<Window*>(glfwGetWindowUserPointer(Handle));
        if (!Self) return;
        Self->_Width   = CAST<u32>(W);
        Self->_Height  = CAST<u32>(H);
        Self->_Resized = true;
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