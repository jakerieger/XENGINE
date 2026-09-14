//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once

#include "EngineCommon.hpp"

#include <functional>
#include <string>

struct GLFWwindow;

namespace Xen {
    class Window {
    public:
        enum class Mode : u8 {
            Windowed   = 0,
            Borderless = 1,
            Fullscreen = 2,
        };

        Window(const std::string& Title, Mode WindowMode, u32 Width, u32 Height);
        ~Window();

        Window(const Window&)            = delete;
        Window& operator=(const Window&) = delete;

        void PollEvents() const;
        void SwapBuffers() const;
        void Clear(bool Depth = false);

        _NoDiscard bool ShouldClose() const;

        _NoDiscard u32 GetWidth() const { return _Width; }
        _NoDiscard u32 GetHeight() const { return _Height; }
        _NoDiscard bool IsMinimized() const { return _Width == 0 || _Height == 0; }
        _NoDiscard GLFWwindow* GetHandle() const { return _Handle; }

        _NoDiscard bool ConsumeResized();

    private:
        static void OnFramebufferResized(GLFWwindow* Handle, int W, int H);
        void Shutdown() const;

        /// @brief Center the window on the active monitor.
        void CenterWindowOnScreen() const;

        GLFWwindow* _Handle {nullptr};
        u32 _Width {0};
        u32 _Height {0};
        bool _Resized {false};
    };
}  // namespace Xen
