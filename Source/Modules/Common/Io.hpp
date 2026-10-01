//
// Created by Jake Rieger on 10/1/2026.
//

#pragma once

#include "XenCommon.hpp"

#include <span>

/// @brief Basic file I/O helper methods. Uses C-style FILE API instead of STL file streams for speed.
namespace Xen::IO {
    DEFINE_ENGINE_EXCEPTION(IOException);

    void WriteString(const std::string& Str, const std::filesystem::path& Path);
    void WriteWideString(const std::wstring& Str, const std::filesystem::path& Path);
    void WriteBytes(std::span<const u8> Bytes, const std::filesystem::path& Path);
    void WriteBytes(const u8* Bytes, size_t Size, const std::filesystem::path& Path);

    std::string ReadString(const std::filesystem::path& Path);
    std::wstring ReadWideString(const std::filesystem::path& Path);
    std::vector<u8> ReadBytes(const std::filesystem::path& Path);
}  // namespace Xen::IO
