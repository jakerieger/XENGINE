//
// Created by Jake Rieger on 10/6/2026.
//

#include "ContentBrowser.hpp"
#include "UI.hpp"

#include <Common/Log.hpp>

#include <shellapi.h>

#include <algorithm>
#include <cctype>

namespace Xen {
    namespace {
        constexpr double kPollIntervalSeconds = 1.0;

        // A file name's characters Windows won't accept, and the trailing
        // space/dot it silently strips.
        std::string ValidateName(const std::string& Name) {
            if (Name.empty()) return "The name can't be empty.";
            if (Name.size() > 200) return "The name is too long.";
            if (Name.find_first_of("<>:\"/\\|?*") != std::string::npos) {
                return "A name can't contain any of  < > : \" / \\ | ? *";
            }
            if (Name.back() == ' ' || Name.back() == '.') return "A name can't end with a space or a dot.";
            return {};
        }

        // "<name> copy", "<name> copy 2", ... - the first that isn't taken.
        fs::path UniqueCopyPath(const fs::path& Source, const bool IsDirectory) {
            const fs::path Dir      = Source.parent_path();
            const std::string Stem  = IsDirectory ? Source.filename().string() : Source.stem().string();
            const std::string Ext   = IsDirectory ? std::string() : Source.extension().string();

            for (int N = 1;; ++N) {
                const std::string Name = Stem + " copy" + (N > 1 ? " " + std::to_string(N) : std::string()) + Ext;
                if (fs::path Candidate = Dir / Name; !fs::exists(Candidate)) return Candidate;
            }
        }

        // To the Recycle Bin rather than gone for good - these are the
        // user's project files, and nothing here tracks references to them.
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

        std::string Lowercase(std::string S) {
            std::ranges::transform(S, S.begin(), [](const unsigned char C) { return CAST<char>(std::tolower(C)); });
            return S;
        }

        EditorIcon IconFor(const AssetKind Kind) {
            switch (Kind) {
                case AssetKind::Mesh:
                    return EditorIcon::ContentMesh;
                case AssetKind::Scene:
                    return EditorIcon::ContentScene;
                case AssetKind::Audio:
                    return EditorIcon::ContentAudio;
                case AssetKind::Prefab:
                    return EditorIcon::ContentPrefab;
                // No dedicated texture/data thumbnail yet.
                default:
                    return EditorIcon::ContentPlainText;
            }
        }
    }  // namespace

    void ContentBrowser::SetRoot(const fs::path& ContentRoot) {
        _Root       = ContentRoot;
        _CurrentDir = ContentRoot;
        _Selected.clear();
        _Items.clear();
        _ListingSignature.clear();
        _Filter[0]    = '\0';
        _NeedsRefresh = true;
    }

    void ContentBrowser::Navigate(const fs::path& Dir) {
        _CurrentDir = Dir;
        _Selected.clear();
        _NeedsRefresh = true;
    }

    void ContentBrowser::Refresh(AssetIndex& Index) {
        _Items.clear();

        std::error_code Error;
        if (!fs::is_directory(_CurrentDir, Error)) _CurrentDir = _Root;

        for (const auto& DirEntry : fs::directory_iterator(_CurrentDir, Error)) {
            Item It;
            It.Path        = DirEntry.path();
            It.Name        = It.Path.filename().string();
            It.IsDirectory = DirEntry.is_directory(Error);

            if (It.IsDirectory) {
                It.IsEmptyDirectory = fs::directory_iterator(It.Path, Error) == fs::directory_iterator();
            } else if (DirEntry.is_regular_file(Error)) {
                It.Asset.RelativePath = fs::relative(It.Path, _Root, Error);
                It.Asset.ID           = AssetIDFromRelativePath(It.Asset.RelativePath);
                It.Asset.Name         = It.Name;
                It.Asset.Kind         = AssetKindFromExtension(It.Path);
            } else {
                continue;
            }
            _Items.push_back(std::move(It));
        }

        std::ranges::sort(_Items, [](const Item& A, const Item& B) {
            if (A.IsDirectory != B.IsDirectory) return A.IsDirectory;
            return Lowercase(A.Name) < Lowercase(B.Name);
        });

        // Cheap change detection for the poll in Draw: the current folder's
        // names. A change also means the index (recursive) may be stale.
        std::vector<std::string> Signature;
        Signature.reserve(_Items.size());
        for (const Item& It : _Items)
            Signature.push_back(It.Name);

        if (Signature != _ListingSignature || Index.Root() != _Root) {
            _ListingSignature = std::move(Signature);
            Index.Rescan(_Root);
        }
        _NeedsRefresh = false;
    }

    void ContentBrowser::MarkChanged() {
        _NeedsRefresh = true;
        _ListingSignature.clear();  // forces an index rescan
    }

    void ContentBrowser::ImportAsset() {
        const auto AssetFile = FileDialogs::OpenFileDialog(nullptr,
                                                           L"Import Asset",
                                                           {{
                                                             .Name       = L"Asset File",
                                                             .Extensions = L"*.*",
                                                           }},
                                                           _CurrentDir);
        if (!AssetFile.has_value()) return;

        // Copy file to current directory and refresh browser
        const fs::path Destination = _CurrentDir / AssetFile->filename();

        std::error_code Error;
        if (fs::equivalent(*AssetFile, Destination, Error)) return;  // already here

        fs::copy_file(*AssetFile, Destination, fs::copy_options::overwrite_existing, Error);
        if (Error) {
            LOG_ERR("Import Asset: couldn't copy '%s' to '%s': %s",
                    AssetFile->string().c_str(),
                    Destination.string().c_str(),
                    Error.message().c_str());
            return;
        }

        MarkChanged();
    }

    void ContentBrowser::DuplicateItem(const Item& It) {
        const fs::path Destination = UniqueCopyPath(It.Path, It.IsDirectory);

        std::error_code Error;
        if (It.IsDirectory) fs::copy(It.Path, Destination, fs::copy_options::recursive, Error);
        else fs::copy_file(It.Path, Destination, Error);

        if (Error) {
            LOG_ERR("Duplicate: couldn't copy '%s': %s", It.Path.string().c_str(), Error.message().c_str());
            return;
        }

        _Selected = Destination;
        MarkChanged();
    }

    void ContentBrowser::DrawItemMenu(const Item& It,
                                      const std::function<void(const fs::path&)>& InstantiatePrefab) {
        if (!It.IsDirectory && It.Asset.Kind == AssetKind::Prefab && InstantiatePrefab) {
            if (ImGui::MenuItem("Instantiate in Scene")) InstantiatePrefab(It.Path);
            ImGui::Separator();
        }

        if (ImGui::MenuItem("Rename")) {
            _PendingOp          = PendingOp::Rename;
            _PendingTarget      = It.Path;
            _PendingIsDirectory = It.IsDirectory;
            _RenameError.clear();

            // Files are renamed without their extension, which decides what
            // kind of asset they are.
            const std::string Stem = It.IsDirectory ? It.Name : It.Path.stem().string();
            std::snprintf(_RenameBuffer, sizeof(_RenameBuffer), "%s", Stem.c_str());
            _OpenPendingPopup = true;
        }

        if (ImGui::MenuItem("Duplicate")) DuplicateItem(It);

        ImGui::Separator();

        if (ImGui::MenuItem("Delete")) {
            _PendingOp          = PendingOp::Delete;
            _PendingTarget      = It.Path;
            _PendingIsDirectory = It.IsDirectory;
            _OpenPendingPopup   = true;
        }
    }

    void ContentBrowser::DrawRenameModal() {
        UI::CenterNextWindow();
        if (!ImGui::BeginPopupModal("Rename", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

        const std::string Extension = _PendingIsDirectory ? std::string() : _PendingTarget.extension().string();

        ImGui::Text("Rename '%s'", _PendingTarget.filename().string().c_str());
        ImGui::Spacing();

        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(260.0f);
        const bool Submitted = ImGui::InputText("##newname",
                                                _RenameBuffer,
                                                sizeof(_RenameBuffer),
                                                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        if (!Extension.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", Extension.c_str());
        }

        // Re-checked every frame so the message follows what's typed.
        const std::string NewName    = std::string(_RenameBuffer) + Extension;
        const fs::path Destination   = _PendingTarget.parent_path() / NewName;
        std::string Problem          = ValidateName(_RenameBuffer);
        const bool OnlyCaseChanged   = Lowercase(NewName) == Lowercase(_PendingTarget.filename().string());
        if (Problem.empty() && NewName != _PendingTarget.filename().string() && !OnlyCaseChanged &&
            fs::exists(Destination)) {
            Problem = "'" + NewName + "' already exists here.";
        }

        ImGui::TextDisabled("References to it in scenes and components will show as Missing.");
        if (!Problem.empty()) ImGui::TextColored(ImVec4(0.92f, 0.22f, 0.32f, 1.0f), "%s", Problem.c_str());
        if (!_RenameError.empty()) ImGui::TextColored(ImVec4(0.92f, 0.22f, 0.32f, 1.0f), "%s", _RenameError.c_str());
        ImGui::Spacing();

        const bool Unchanged = NewName == _PendingTarget.filename().string();
        ImGui::BeginDisabled(!Problem.empty() || Unchanged);
        const bool Confirm = ImGui::Button("Rename", ImVec2(120, 0)) || (Submitted && Problem.empty() && !Unchanged);
        ImGui::EndDisabled();
        ImGui::SameLine();
        const bool Cancel = ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false);

        if (Confirm) {
            std::error_code Error;
            fs::rename(_PendingTarget, Destination, Error);
            if (Error) {
                _RenameError = "Couldn't rename: " + Error.message();
            } else {
                _Selected  = Destination;
                _PendingOp = PendingOp::None;
                MarkChanged();
                ImGui::CloseCurrentPopup();
            }
        } else if (Cancel) {
            _PendingOp = PendingOp::None;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    void ContentBrowser::DrawDeleteModal() {
        UI::CenterNextWindow();
        if (!ImGui::BeginPopupModal("Delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

        ImGui::Text("Delete '%s'?", _PendingTarget.filename().string().c_str());
        if (_PendingIsDirectory) ImGui::TextUnformatted("Everything inside the folder goes with it.");
        ImGui::TextDisabled("It's moved to the Recycle Bin. References to it in scenes and\ncomponents will show as Missing.");
        ImGui::Spacing();

        const bool Confirm = ImGui::Button("Delete", ImVec2(120, 0));
        ImGui::SameLine();
        const bool Cancel = ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false);

        if (Confirm) {
            if (MoveToRecycleBin(_PendingTarget)) {
                if (_Selected == _PendingTarget) _Selected.clear();
                MarkChanged();
            } else {
                LOG_ERR("Delete: couldn't move '%s' to the Recycle Bin", _PendingTarget.string().c_str());
            }
            _PendingOp = PendingOp::None;
            ImGui::CloseCurrentPopup();
        } else if (Cancel) {
            _PendingOp = PendingOp::None;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    void ContentBrowser::DrawToolbar() {
        const ImGuiStyle& Style = ImGui::GetStyle();

        if (ImGui::Button("Import Asset")) ImportAsset();
        ImGui::SameLine();

        ImGui::BeginDisabled(_CurrentDir == _Root);
        if (UI::Controls::TransparentButton("<")) Navigate(_CurrentDir.parent_path());
        ImGui::EndDisabled();
        ImGui::SameLine();

        // Breadcrumbs: Content > sub > folder
        fs::path Walk = _Root;
        if (UI::Controls::TransparentButton(_Root.filename().string().c_str())) Navigate(_Root);
        const fs::path Relative = _CurrentDir.lexically_relative(_Root);
        for (const fs::path& Part : Relative) {
            if (Part.empty() || Part == ".") continue;
            Walk /= Part;
            ImGui::SameLine(0.0f, Style.ItemInnerSpacing.x);
            ImGui::TextDisabled(">");
            ImGui::SameLine(0.0f, Style.ItemInnerSpacing.x);
            ImGui::PushID(Walk.string().c_str());
            if (UI::Controls::TransparentButton(Part.string().c_str())) Navigate(Walk);
            ImGui::PopID();
        }

        // Right-aligned controls: filter, tile size, refresh.
        const f32 FilterWidth = 160.0f;
        const f32 SliderWidth = 90.0f;
        const f32 Total = FilterWidth + SliderWidth + ImGui::CalcTextSize("Refresh").x + Style.FramePadding.x * 2.0f +
                          Style.ItemSpacing.x * 3.0f;
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - Total);

        ImGui::SetNextItemWidth(FilterWidth);
        ImGui::InputTextWithHint("##filter", "Filter", _Filter, sizeof(_Filter));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(SliderWidth);
        ImGui::SliderFloat("##tilesize", &_TileSize, 56.0f, 160.0f, "");
        ImGui::SameLine();
        if (ImGui::Button("Refresh")) MarkChanged();
    }

    void ContentBrowser::Draw(const IconLibrary& Icons,
                              AssetIndex& Index,
                              const std::function<void(const fs::path&)>& OpenScene,
                              const std::function<void(const fs::path&)>& InstantiatePrefab) {
        if (!ImGui::Begin("Content Browser")) {
            ImGui::End();
            return;
        }

        if (_Root.empty() || !fs::is_directory(_Root)) {
            ImGui::TextDisabled("No project content directory.");
            ImGui::End();
            return;
        }

        const double Now = ImGui::GetTime();
        if (_NeedsRefresh || Now - _LastPollTime > kPollIntervalSeconds) {
            _LastPollTime = Now;
            Refresh(Index);
        }

        DrawToolbar();
        ImGui::Separator();

        if (ImGui::BeginChild("##grid", ImVec2(0, 0), ImGuiChildFlags_None)) {
            const ImGuiStyle& Style = ImGui::GetStyle();
            const f32 Cell          = _TileSize;
            const f32 Avail         = ImGui::GetContentRegionAvail().x;
            const i32 Columns       = std::max(1, CAST<i32>(Avail / (Cell + Style.ItemSpacing.x)));

            const std::string Filter   = Lowercase(_Filter);
            const fs::path* Navigating = nullptr;
            i32 Shown                  = 0;

            for (const Item& It : _Items) {
                if (!Filter.empty() && Lowercase(It.Name).find(Filter) == std::string::npos) continue;

                if (Shown % Columns != 0) ImGui::SameLine();
                ++Shown;

                const EditorIcon IconKind =
                  It.IsDirectory ? (It.IsEmptyDirectory ? EditorIcon::ContentFolderEmpty : EditorIcon::ContentFolder)
                                 : IconFor(It.Asset.Kind);

                const UI::Controls::AssetTileResult Tile = UI::Controls::AssetTile(It.Name.c_str(),
                                                                                   Icons.Get(IconKind),
                                                                                   It.Name.c_str(),
                                                                                   Cell,
                                                                                   _Selected == It.Path);

                if (Tile.Clicked) _Selected = It.Path;
                if (!It.IsDirectory) UI::Controls::AssetDragSource(It.Asset);

                // Right-click: select it, and offer what can be done to it.
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) _Selected = It.Path;
                if (ImGui::BeginPopupContextItem(("##itemmenu" + It.Name).c_str())) {
                    DrawItemMenu(It, InstantiatePrefab);
                    ImGui::EndPopup();
                }

                if (Tile.DoubleClicked) {
                    if (It.IsDirectory) {
                        Navigating = &It.Path;
                    } else if (It.Asset.Kind == AssetKind::Scene && OpenScene) {
                        OpenScene(It.Path);
                    }
                }
            }

            if (Shown == 0) ImGui::TextDisabled(_Items.empty() ? "This folder is empty." : "No matches.");

            // Right-click on empty space (not over a tile).
            if (ImGui::BeginPopupContextWindow("##gridmenu",
                                               ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
                if (ImGui::MenuItem("Import Asset")) ImportAsset();
                ImGui::EndPopup();
            }

            // Deselect on a click in empty space.
            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                !ImGui::IsAnyItemHovered()) {
                _Selected.clear();
            }

            // Deferred until the loop is done: Navigate() rebuilds _Items.
            if (Navigating) {
                const fs::path Target = *Navigating;
                Navigate(Target);
            }
        }
        ImGui::EndChild();

        // OpenPopup and BeginPopupModal have to agree on the id stack, which
        // is why the item menu only records the request and it's opened here.
        if (_OpenPendingPopup) {
            ImGui::OpenPopup(_PendingOp == PendingOp::Rename ? "Rename" : "Delete");
            _OpenPendingPopup = false;
        }
        DrawRenameModal();
        DrawDeleteModal();

        ImGui::End();
    }
}  // namespace Xen
