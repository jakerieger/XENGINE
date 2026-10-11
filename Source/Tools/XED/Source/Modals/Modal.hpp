//
// Created by Jake Rieger on 10/12/2026.
//
// Base for the editor's modal dialogs: a modal is a small class that owns its
// own state, is opened with an Open(...) of its own, and is drawn once per
// frame with Draw(). Open only records the request - the popup itself is
// opened inside Draw, at the same place it's begun, so the two always agree
// on the ImGui id stack no matter which panel asked for it.

#pragma once

#include <Common/XenCommon.hpp>

#include <imgui.h>

#include <string>

namespace Xen {
    class Modal {
    public:
        virtual ~Modal() = default;

        /// @brief Draws the dialog if it's open (and opens it first, if Open
        /// was just called). Call every frame, from wherever the dialog should
        /// live in the ImGui id stack - typically the top level of the editor
        /// UI, or inside the panel that owns it.
        void Draw();

    protected:
        /// Called by a derived class's Open(...) once its state is set up.
        void RequestOpen() { _RequestOpen = true; }

        /// The popup's name: its title, and the id it's opened by. Unique per
        /// modal that can be on screen at once.
        NODISCARD virtual const char* Title() const = 0;

        NODISCARD virtual ImGuiWindowFlags Flags() const { return ImGuiWindowFlags_AlwaysAutoResize; }

        /// Everything inside the popup. Call ImGui::CloseCurrentPopup() to
        /// close it. Only called while the dialog is open.
        virtual void DrawBody() = 0;

        // Coloured one-liners for validation and failure messages.
        static void ErrorText(const std::string& Message);
        static void WarningText(const std::string& Message);

    private:
        bool _RequestOpen {false};
    };

    /// @brief Why Name can't be a file or folder name on Windows, or an empty
    /// string if it can. Checks characters, length and a trailing space/dot -
    /// not whether the name is taken, which depends on where it's going.
    std::string ValidateFileName(const std::string& Name);
}  // namespace Xen
