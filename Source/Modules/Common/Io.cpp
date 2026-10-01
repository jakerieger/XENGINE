//
// Created by Jake Rieger on 10/1/2026.
//

#include "Io.hpp"
#include <filesystem>

namespace Xen {
    namespace {
        struct FILECloser {
            void operator()(FILE* F) const noexcept {
                if (F) std::fclose(F);
            }
        };

        using FilePtr = std::unique_ptr<FILE, FILECloser>;
    }  // namespace

    void IO::WriteString(const std::string& Str, const std::filesystem::path& Path) {
        const FilePtr F(std::fopen(Path.string().c_str(), "w"));
        if (!F) { THROW_ENGINE_EXCEPTION(IOException, std::format("Failed to open: '{}'", Path.string())); }

        if (fputs(Str.c_str(), F.get()) != 0) {
            THROW_ENGINE_EXCEPTION(IOException, std::format("Failed to write to: '{}'", Path.string()));
        }
    }

    void IO::WriteWideString(const std::wstring& Str, const std::filesystem::path& Path) {
        const FilePtr F(std::fopen(Path.string().c_str(), "w"));
        if (!F) { THROW_ENGINE_EXCEPTION(IOException, std::format("Failed to open: '{}'", Path.string())); }

        if (std::fputws(Str.c_str(), F.get()) != 0) {
            THROW_ENGINE_EXCEPTION(IOException, std::format("Failed to write to: '{}'", Path.string()));
        }
    }

    void IO::WriteBytes(const std::span<const u8> Bytes, const std::filesystem::path& Path) {
        WriteBytes(Bytes.data(), Bytes.size(), Path);
    }

    void IO::WriteBytes(const u8* Bytes, const size_t Size, const std::filesystem::path& Path) {
        const FilePtr F(std::fopen(Path.string().c_str(), "wb"));
        if (!F) { THROW_ENGINE_EXCEPTION(IOException, std::format("Failed to open: '{}'", Path.string())); }

        const size_t BytesWritten = std::fwrite(Bytes, sizeof(u8), Size, F.get());
        if (BytesWritten != Size) {
            THROW_ENGINE_EXCEPTION(IOException, std::format("Failed to write to: '{}'", Path.string()));
        }
    }

    std::string IO::ReadString(const std::filesystem::path& Path) {
        if (!exists(Path)) {
            THROW_ENGINE_EXCEPTION(IOException, std::format("File does not exist: '{}'", Path.string()));
        }

        const FilePtr F(std::fopen(Path.string().c_str(), "r"));
        if (!F) { THROW_ENGINE_EXCEPTION(IOException, std::format("Failed to open: '{}'", Path.string())); }

        if (std::fseek(F.get(), 0, SEEK_END) != 0) {
            THROW_ENGINE_EXCEPTION(IOException, std::format("fseek failed '{}'", Path.string()));
        }
        const size_t Size = CAST<size_t>(std::ftell(F.get()));
        std::rewind(F.get());

        std::string Str;
        if (Size <= 0) { THROW_ENGINE_EXCEPTION(IOException, std::format("File size <= 0: '{}'", Path.string())); }

        Str.resize(Size);
        const size_t BytesRead = std::fread(Str.data(), 1, Size, F.get());
        if (BytesRead < Size) { Str.resize(BytesRead); }

        return Str;
    }

    std::wstring IO::ReadWideString(const std::filesystem::path& Path) {
        if (!exists(Path)) {
            THROW_ENGINE_EXCEPTION(IOException, std::format("File does not exist: '{}'", Path.string()));
        }

        const FilePtr F(std::fopen(Path.string().c_str(), "r"));
        if (!F) { THROW_ENGINE_EXCEPTION(IOException, std::format("Failed to open: '{}'", Path.string())); }

        // Set the locale to the system default so fgetwc handles multibyte parsing correctly
        std::setlocale(LC_ALL, "");

        std::wstring Str;
        wint_t Wc;

        while ((Wc = std::fgetwc(F.get())) != WEOF) {
            Str.push_back(CAST<wchar_t>(Wc));
        }

        return Str;
    }

    std::vector<u8> IO::ReadBytes(const std::filesystem::path& Path) {
        if (!exists(Path)) {
            THROW_ENGINE_EXCEPTION(IOException, std::format("File does not exist: '{}'", Path.string()));
        }

        const FilePtr F(std::fopen(Path.string().c_str(), "rb"));
        if (!F) { THROW_ENGINE_EXCEPTION(IOException, std::format("Failed to open: '{}'", Path.string())); }

        if (std::fseek(F.get(), 0, SEEK_END) != 0) {
            THROW_ENGINE_EXCEPTION(IOException, std::format("fseek failed '{}'", Path.string()));
        }
        const size_t Size = CAST<size_t>(std::ftell(F.get()));
        std::rewind(F.get());

        std::vector<u8> Bytes(Size);
        const size_t N = std::fread(Bytes.data(), 1, Bytes.size(), F.get());
        if (N != Bytes.size() && std::ferror(F.get())) {
            THROW_ENGINE_EXCEPTION(IOException, std::format("fread failed '{}'", Path.string()));
        }
        Bytes.resize(N);

        return Bytes;
    }
}  // namespace Xen