//
// Created by Jake Rieger on 10/6/2026.
//
// The "Content Browser" panel: a thumbnail grid of the project's Content
// directory (folders first), with a breadcrumb bar, name filter and tile size
// slider. Files are drag sources for the inspector's asset slots (see
// UI::Controls::AssetSlot). Right-clicking an item offers Rename, Duplicate
// and Delete (to the Recycle Bin); right-clicking empty space offers Import
// Asset, the same action as the toolbar button.

#pragma once

#include "AssetIndex.hpp"
#include "IconLibrary.hpp"

#include <functional>
#include <string>
#include <vector>

namespace Xen {
    class ContentBrowser {
    public:
        /// @brief Points the browser at a project's content directory and
        /// returns it to the root. Call with an empty path when no project is
        /// open.
        void SetRoot(const fs::path& ContentRoot);

        /// @brief Draws the panel. Index is rescanned here when the browser
        /// notices the directory changed on disk (or Refresh is pressed), so
        /// inspector asset slots resolve newly added files. OpenScene is
        /// called when a scene file is double-clicked; InstantiatePrefab when
        /// a prefab's "Instantiate in Scene" menu entry is chosen.
        void Draw(const IconLibrary& Icons,
                  AssetIndex& Index,
                  const std::function<void(const fs::path&)>& OpenScene,
                  const std::function<void(const fs::path&)>& InstantiatePrefab);

        /// @brief Something outside the browser added, removed or changed
        /// files in the content directory: re-read the folder and rescan the
        /// asset index on the next Draw.
        void NotifyContentChanged() { MarkChanged(); }

    private:
        struct Item {
            fs::path Path;
            std::string Name;
            bool IsDirectory {false};
            bool IsEmptyDirectory {false};
            AssetEntry Asset;  // files only
        };

        void Navigate(const fs::path& Dir);
        void Refresh(AssetIndex& Index);
        void DrawToolbar();

        /// Asks for a file and copies it into the current folder. Shared by
        /// the toolbar button and the empty-space context menu.
        void ImportAsset();

        /// Right-click menu body for one item (inside its popup).
        void DrawItemMenu(const Item& It, const std::function<void(const fs::path&)>& InstantiatePrefab);

        /// Copies an item next to itself as "<name> copy" (numbered if taken).
        void DuplicateItem(const Item& It);

        /// Modals for the operations that need the user's input or consent.
        /// Opened by DrawItemMenu through _PendingOp, drawn once per frame
        /// from Draw.
        void DrawRenameModal();
        void DrawDeleteModal();

        /// Makes the next Draw re-read the folder, and the asset index
        /// (anything that changes the files on disk).
        void MarkChanged();

        fs::path _Root;
        fs::path _CurrentDir;
        fs::path _Selected;
        std::vector<Item> _Items;
        std::vector<std::string> _ListingSignature;

        // The operation an item's menu asked for, awaiting its modal.
        enum class PendingOp : u8 { None, Rename, Delete };
        PendingOp _PendingOp {PendingOp::None};
        bool _OpenPendingPopup {false};
        fs::path _PendingTarget;
        bool _PendingIsDirectory {false};
        char _RenameBuffer[256] {};
        std::string _RenameError;

        char _Filter[128] {};
        float _TileSize {96.0f};
        double _LastPollTime {0.0};
        bool _NeedsRefresh {true};
    };
}  // namespace Xen
