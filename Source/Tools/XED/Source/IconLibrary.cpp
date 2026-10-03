//
// Created by Jake Rieger on 9/29/2026.
//

#include "IconLibrary.hpp"
#include "EditorUI.hpp"
#include "Resource/EditorIcons.h"

#include <Common/Brotli.hpp>
#include <Common/Log.hpp>

#include <iterator>

namespace Xen {
    namespace {
        struct IconDesc {
            EditorIcon Icon;
            const unsigned char* Bytes;
            size_t CompressedSize;
            size_t OriginalSize;
            u32 Width;
            u32 Height;
        };

        // EnumName is this table's EditorIcon enumerator; SymbolName is the
        // matching ResTool-generated identifier (EditorIcons.h) - the two
        // differ in case/spelling (e.g. EditorIcon::Move vs. MOVEICON_BYTES)
        // since ResTool just uppercases a source filename's stem.
#define XED_ICON_ENTRY(EnumName, SymbolName)                                                                           \
    {EditorIcon::EnumName,                                                                                             \
     SymbolName##_BYTES,                                                                                               \
     SymbolName##_COMPRESSED_SIZE,                                                                                     \
     SymbolName##_ORIGINAL_SIZE,                                                                                       \
     CAST<u32>(SymbolName##_WIDTH),                                                                                    \
     CAST<u32>(SymbolName##_HEIGHT)}

        constexpr IconDesc kIconTable[] = {
          XED_ICON_ENTRY(CleanCode, CLEANCODE),
          XED_ICON_ENTRY(CompileCode, COMPILECODE),
          XED_ICON_ENTRY(FocusSelected, FOCUSSELECTED),
          XED_ICON_ENTRY(GridToggle, GRIDTOGGLE),
          XED_ICON_ENTRY(Move, MOVEICON),
          XED_ICON_ENTRY(OpenFolder, OPENFOLDER),
          XED_ICON_ENTRY(Pause, PAUSEICON),
          XED_ICON_ENTRY(Play, PLAYICON),
          XED_ICON_ENTRY(PlayWindowed, PLAYWINDOWEDICON),
          XED_ICON_ENTRY(Redo, REDOICON),
          XED_ICON_ENTRY(Rotate, ROTATEICON),
          XED_ICON_ENTRY(Scale, SCALEICON),
          XED_ICON_ENTRY(SelectAsset, SELECTASSETICON),
          XED_ICON_ENTRY(Select, SELECTICON),
          XED_ICON_ENTRY(Stop, STOPICON),
          XED_ICON_ENTRY(Undo, UNDOICON),
        };
#undef XED_ICON_ENTRY

        static_assert(std::size(kIconTable) == CAST<size_t>(EditorIcon::Count),
                      "kIconTable must have exactly one entry per EditorIcon");
    }  // namespace

    bool IconLibrary::Initialize(RHI::IRenderDevice& Device, EditorUI& UI) {
        if (!UI.IsInitialized()) return false;

        _Device = &Device;

        for (const IconDesc& Desc : kIconTable) {
            const auto Pixels = Brotli::Decompress(std::span(Desc.Bytes, Desc.CompressedSize), Desc.OriginalSize);
            if (!Pixels.has_value()) {
                LOG_ERR("IconLibrary: failed to decompress icon %u", CAST<u32>(Desc.Icon));
                continue;
            }

            RHI::TextureDesc TexDesc;
            TexDesc.Type      = RHI::TextureType::Texture2D;
            TexDesc.Fmt       = RHI::Format::RGBA8_UNORM;
            TexDesc.Width     = Desc.Width;
            TexDesc.Height    = Desc.Height;
            TexDesc.MipLevels = 1;
            TexDesc.Usage     = RHI::TextureUsage::Sampled | RHI::TextureUsage::CopyDst;

            const RHI::TextureHandle Handle = _Device->CreateTexture(TexDesc);
            if (!Handle.IsValid()) {
                LOG_ERR("IconLibrary: GPU texture creation failed for icon %u", CAST<u32>(Desc.Icon));
                continue;
            }

            RHI::TextureUploadDesc Upload;
            Upload.Data     = Pixels->data();
            Upload.DataSize = Pixels->size();
            Upload.Width    = Desc.Width;
            Upload.Height   = Desc.Height;
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
