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
        void SwapBuffers() const;
        void Clear(bool Depth = false);

        NODISCARD bool ShouldClose() const;

        NODISCARD u32 GetWidth() const { return _Width; }
        NODISCARD u32 GetHeight() const { return _Height; }
        NODISCARD bool IsMinimized() const { return _Width == 0 || _Height == 0; }
        NODISCARD GLFWwindow* GetHandle() const { return _Handle; }
        NODISCARD InputManager& GetInputManager() { return _InputManager; }

        NODISCARD bool ConsumeResized();

        void ResetInput();

    private:
        static void OnFrameBufferResized(GLFWwindow* Handle, int W, int H);
        static void OnKeyCallback(GLFWwindow* Handle, int Key, int ScanCode, int Action, int Mods);
        static void OnMouseButtonCallback(GLFWwindow* Handle, int Button, int Action, int Mods);
        static void OnCursorPosCallback(GLFWwindow* Handle, double X, double Y);
        static void OnMouseScrollCallback(GLFWwindow* Handle, double DeltaX, double DeltaY);

        void Shutdown() const;

        /// @brief Center the window on the active monitor.
        void CenterWindowOnScreen() const;

        GLFWwindow* _Handle {nullptr};
        u32 _Width {0};
        u32 _Height {0};
        bool _Resized {false};
        InputManager _InputManager;
    };
}  // namespace Xen
