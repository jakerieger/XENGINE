//
// Created by Jake Rieger on 10/12/2026.
//

#include "Modal.hpp"
#include "../UI.hpp"

namespace Xen {
    void Modal::Draw() {
        if (_RequestOpen) {
            ImGui::OpenPopup(Title());
            _RequestOpen = false;
        }

        UI::CenterNextWindow();
        if (!ImGui::BeginPopupModal(Title(), nullptr, Flags())) return;

        DrawBody();

        ImGui::EndPopup();
    }

    void Modal::ErrorText(const std::string& Message) {
        ImGui::TextColored(ImVec4(0.92f, 0.22f, 0.32f, 1.0f), "%s", Message.c_str());
    }

    void Modal::WarningText(const std::string& Message) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", Message.c_str());
    }

    std::string ValidateFileName(const std::string& Name) {
        if (Name.empty()) return "The name can't be empty.";
        if (Name.size() > 200) return "The name is too long.";
        if (Name.find_first_of("<>:\"/\\|?*") != std::string::npos) {
            return "A name can't contain any of  < > : \" / \\ | ? *";
        }
        if (Name.back() == ' ' || Name.back() == '.') return "A name can't end with a space or a dot.";
        return {};
    }
}  // namespace Xen
