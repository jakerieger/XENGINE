//
// Created by Jake Rieger on 10/12/2026.
//

#include "DeleteModal.hpp"

#include <Common/Log.hpp>

#include <shellapi.h>

namespace Xen {
    namespace {
        bool MoveToRecycleBin(const fs::path& Path) {
            // Backslashes only: SHFileOperation fails on a path with forward ones.
            std::wstring From = fs::absolute(Path).lexically_normal().make_preferred().wstring();
            From.push_back(L'\0');  // SHFileOperation wants a double-null-terminated list

            SHFILEOPSTRUCTW Op {};
            Op.wFunc  = FO_DELETE;
            Op.pFrom  = From.c_str();
            Op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
            return SHFileOperationW(&Op) == 0 && !Op.fAnyOperationsAborted;
        }
    }  // namespace

    void DeleteModal::Open(const fs::path& Target, const bool IsDirectory, Callback OnDeleted) {
        _Target      = Target;
        _IsDirectory = IsDirectory;
        _OnDeleted   = std::move(OnDeleted);
        RequestOpen();
    }

    void DeleteModal::DrawBody() {
        ImGui::Text("Delete '%s'?", _Target.filename().string().c_str());
        if (_IsDirectory) ImGui::TextUnformatted("Everything inside the folder goes with it.");
        ImGui::TextDisabled(
          "It's moved to the Recycle Bin. References to it in scenes and\ncomponents will show as Missing.");
        ImGui::Spacing();

        const bool Confirm = ImGui::Button("Delete", ImVec2(120, 0));
        ImGui::SameLine();
        const bool Cancel = ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false);

        if (Confirm) {
            if (MoveToRecycleBin(_Target)) {
                if (_OnDeleted) _OnDeleted();
            } else {
                LOG_ERR("Delete: couldn't move '%s' to the Recycle Bin", _Target.string().c_str());
            }
            ImGui::CloseCurrentPopup();
        } else if (Cancel) {
            ImGui::CloseCurrentPopup();
        }
    }
}  // namespace Xen
