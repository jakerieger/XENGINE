//
// Created by Jake Rieger on 9/28/2026.
//
// The minimal contract Window needs from whatever ImGui-based overlay (if
// any) is attached to it - Win32 message forwarding and input-capture
// queries, nothing else. Window talks only to this interface, never to a
// concrete type, so it has no dependency on DebugUI (the engine's own
// Debug/Release-gated overlay for standalone games) or on the editor's own
// always-on UI - those are two separate things with two separate lifetimes
// and compile-time gating rules, and Window shouldn't need to know which one
// (if either) it's plugged into.

#pragma once

#include <Common/XenCommon.hpp>

#include <Windows.h>

namespace Xen {
    class IUIOverlay {
    public:
        virtual ~IUIOverlay() = default;

        /// @brief Forwards a Win32 message to the overlay's own input
        /// handling; returns true if it consumed the message.
        virtual bool ProcessMessage(HWND Handle, UINT Msg, WPARAM WParam, LPARAM LParam) = 0;

        /// @brief True while the mouse is over/interacting with the overlay
        /// - Window uses this to withhold raw-input mouse events (and decide
        /// who owns the cursor) from the game so e.g. dragging a UI window
        /// doesn't also spin the game camera.
        NODISCARD virtual bool WantsCaptureMouse() const = 0;

        /// @brief Same as WantsCaptureMouse but for keyboard input.
        NODISCARD virtual bool WantsCaptureKeyboard() const = 0;
    };
}  // namespace Xen
