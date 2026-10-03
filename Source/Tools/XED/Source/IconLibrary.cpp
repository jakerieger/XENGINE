//
// Created by Jake Rieger on 9/29/2026.
//

#include "IconLibrary.hpp"
#include "EditorUI.hpp"
#include "Resource/EditorIcons.h"

#include <Common/Log.hpp>
#include <stb_image.h>

#include <iterator>
#include <memory>

namespace Xen {
    namespace {
        struct IconDesc {
            EditorIcon Icon;
            const unsigned char* Bytes;
            u32 Size;
        };

        // EnumName is this table's EditorIcon enumerator; SymbolName is the
        // matching Bin2CC-generated identifier (EditorIcons.h), which is just
        // the source PNG's filename uppercased (e.g. Move.png -> MOVE_PNG).
#define XED_ICON_ENTRY(EnumName, SymbolName) {EditorIcon::EnumName, SymbolName##_DATA, SymbolName##_SIZE}

        constexpr IconDesc kIconTable[] = {
          XED_ICON_ENTRY(CleanCode, CLEANCODE_PNG),
          XED_ICON_ENTRY(CompileCode, COMPILECODE_PNG),
          XED_ICON_ENTRY(FocusSelected, FOCUSSELECTED_PNG),
          XED_ICON_ENTRY(GridToggle, GRIDTOGGLE_PNG),
          XED_ICON_ENTRY(Move, MOVE_PNG),
          XED_ICON_ENTRY(OpenFolder, OPENFOLDER_PNG),
          XED_ICON_ENTRY(Pause, PAUSE_PNG),
          XED_ICON_ENTRY(Play, PLAY_PNG),
          XED_ICON_ENTRY(PlayWindowed, PLAYWINDOWED_PNG),
          XED_ICON_ENTRY(Redo, REDO_PNG),
          XED_ICON_ENTRY(Rotate, ROTATE_PNG),
          XED_ICON_ENTRY(Scale, SCALE_PNG),
          XED_ICON_ENTRY(SelectAsset, SELECTASSET_PNG),
          XED_ICON_ENTRY(Select, SELECT_PNG),
          XED_ICON_ENTRY(Stop, STOP_PNG),
          XED_ICON_ENTRY(Undo, UNDO_PNG),
        };
#undef XED_ICON_ENTRY

        static_assert(std::size(kIconTable) == CAST<size_t>(EditorIcon::Count),
                      "kIconTable must have exactly one entry per EditorIcon");

        struct StbiDeleter {
            void operator()(stbi_uc* Pixels) const { stbi_image_free(Pixels); }
        };
    }  // namespace

    bool IconLibrary::Initialize(RHI::IRenderDevice& Device, EditorUI& UI) {
        if (!UI.IsInitialized()) return false;

        _Device = &Device;

        for (const IconDesc& Desc : kIconTable) {
            int W = 0, H = 0, Channels = 0;
            const std::unique_ptr<stbi_uc, StbiDeleter> Pixels(
              stbi_load_from_memory(Desc.Bytes, CAST<int>(Desc.Size), &W, &H, &Channels, STBI_rgb_alpha));
            if (!Pixels) {
                LOG_ERR("IconLibrary: failed to decode icon %u: %s", CAST<u32>(Desc.Icon), stbi_failure_reason());
                continue;
            }

            RHI::TextureDesc TexDesc;
            TexDesc.Type      = RHI::TextureType::Texture2D;
            TexDesc.Fmt       = RHI::Format::RGBA8_UNORM;
            TexDesc.Width     = CAST<u32>(W);
            TexDesc.Height    = CAST<u32>(H);
            TexDesc.MipLevels = 1;
            TexDesc.Usage     = RHI::TextureUsage::Sampled | RHI::TextureUsage::CopyDst;

            const RHI::TextureHandle Handle = _Device->CreateTexture(TexDesc);
            if (!Handle.IsValid()) {
                LOG_ERR("IconLibrary: GPU texture creation failed for icon %u", CAST<u32>(Desc.Icon));
                continue;
            }

            RHI::TextureUploadDesc Upload;
            Upload.Data     = Pixels.get();
            Upload.DataSize = CAST<size_t>(W) * CAST<size_t>(H) * 4;
            Upload.Width    = CAST<u32>(W);
            Upload.Height   = CAST<u32>(H);
            _Device->UploadTexture(Handle, Upload);

            const size_t Index = CAST<size_t>(Desc.Icon);
            _Textures[Index]   = Handle;
            _TextureIDs[Index] = UI.CreateStaticTextureID(Handle);
        }

        return true;
    }

    void IconLibrary::Shutdown() {
        if (!_Device) return;

        for (const RHI::TextureHandle Handle : _Textures) {
            if (Handle.IsValid()) _Device->DestroyTexture(Handle);
        }

        _Textures   = {};
        _TextureIDs = {};
        _Device     = nullptr;
    }

    ImTextureID IconLibrary::Get(const EditorIcon Icon) const {
        return _TextureIDs[CAST<size_t>(Icon)];
    }
}  // namespace Xen
