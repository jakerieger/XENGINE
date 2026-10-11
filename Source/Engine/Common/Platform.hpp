//
// Created by Jake Rieger on 9/17/2026.
//

#pragma once
#include <filesystem>

/// The Xen core library defines these at compile time. Other projects including this header and not linking against
/// Xen::Xen may not, so they're defined here just in case.
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN 1
#endif
#ifndef NOMINMAX
    #define NOMINMAX 1
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
    #define _CRT_SECURE_NO_WARNINGS 1
#endif

#include <Windows.h>
#include <shobjidl.h>
#include <cstdlib>

namespace Xen {
    namespace fs = std::filesystem;

    constexpr unsigned long long operator""_KB(const unsigned long long N) {
        return N * 1024ULL;
    }

    constexpr unsigned long long operator""_MB(const unsigned long long N) {
        return N * 1024ULL * 1024ULL;
    }

    constexpr unsigned long long operator""_GB(const unsigned long long N) {
        return N * 1024ULL * 1024ULL * 1024ULL;
    }

#define KB(N) operator""_KB(N)
#define MB(N) operator""_MB(N)
#define GB(N) operator""_GB(N)

    constexpr double ToKB(const unsigned long long N) {
        return static_cast<double>(N) / 1024.0;
    }

    constexpr double ToMB(const unsigned long long N) {
        return static_cast<double>(N) / 1024.0 / 1024.0;
    }

    constexpr double ToGB(const unsigned long long N) {
        return static_cast<double>(N) / 1024.0 / 1024.0 / 1024.0;
    }

    struct ProcessCommandLineArguments {
        int Argc;
        char** Argv;
    };

    inline bool GetProcessCommandLineArguments(ProcessCommandLineArguments& Arguments) noexcept {
        if (!__p___argc() || !__p___argv()) return false;
        Arguments.Argc = *__p___argc();
        Arguments.Argv = *__p___argv();
        return true;
    }

    inline const char* PathToCStr(const fs::path& Path) noexcept {
        return Path.string().c_str();
    }

    namespace FileDialogs {
        struct FileTypeFilter {
            const wchar_t* Name;
            const wchar_t* Extensions;
        };

        inline void SetStartFolder(IFileOpenDialog* Dialog, const std::filesystem::path& Path) noexcept {
            IShellItem* pFolder = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(Path.wstring().c_str(), nullptr, IID_PPV_ARGS(&pFolder)))) {
                std::ignore = Dialog->SetDefaultFolder(pFolder);
                pFolder->Release();
            }
        }

        inline std::optional<std::filesystem::path>
        OpenFileDialog(const HWND Owner,
                       const std::wstring& Title,
                       const std::vector<FileTypeFilter>& FileFilters,
                       const std::filesystem::path& StartFolder = std::filesystem::current_path()) noexcept {
            IFileOpenDialog* pFileOpen = nullptr;
            auto HR                    = ::CoCreateInstance(CLSID_FileOpenDialog,
                                         nullptr,
                                         CLSCTX_ALL,
                                         IID_IFileOpenDialog,
                                         reinterpret_cast<void**>(&pFileOpen));
            if (FAILED(HR)) { return {}; }

            HR = pFileOpen->SetTitle(Title.c_str());
            if (FAILED(HR)) {
                pFileOpen->Release();
                return {};
            }

            std::vector<COMDLG_FILTERSPEC> FileTypes;
            for (const auto& [Name, Extensions] : FileFilters) {
                FileTypes.push_back({
                  .pszName = Name,
                  .pszSpec = Extensions,
                });
            }

            HR = pFileOpen->SetFileTypes(static_cast<UINT>(FileTypes.size()), FileTypes.data());
            if (FAILED(HR)) {
                pFileOpen->Release();
                return {};
            }

            if (exists(StartFolder)) { SetStartFolder(pFileOpen, StartFolder); }

            HR = pFileOpen->Show(Owner);
            if (FAILED(HR)) {
                pFileOpen->Release();
                return {};
            }

            IShellItem* pItem;
            HR = pFileOpen->GetResult(&pItem);
            if (FAILED(HR)) {
                pFileOpen->Release();
                return {};
            }

            PWSTR FilePath;
            HR = pItem->GetDisplayName(SIGDN_FILESYSPATH, &FilePath);
            if (FAILED(HR)) {
                pItem->Release();
                pFileOpen->Release();
                return {};
            }

            std::filesystem::path OutPath = FilePath;
            ::CoTaskMemFree(FilePath);
            pItem->Release();
            pFileOpen->Release();

            return OutPath;
        }

        inline std::optional<std::filesystem::path>
        OpenFolderDialog(const HWND Owner,
                         const std::wstring& Title,
                         const std::filesystem::path& StartFolder = std::filesystem::current_path()) noexcept {
            IFileOpenDialog* pFileOpen = nullptr;
            auto HR                    = ::CoCreateInstance(CLSID_FileOpenDialog,
                                         nullptr,
                                         CLSCTX_ALL,
                                         IID_IFileOpenDialog,
                                         reinterpret_cast<void**>(&pFileOpen));
            if (FAILED(HR)) return {};

            HR = pFileOpen->SetTitle(Title.c_str());
            if (FAILED(HR)) {
                pFileOpen->Release();
                return {};
            }

            FILEOPENDIALOGOPTIONS Options;
            HR = pFileOpen->GetOptions(&Options);
            if (FAILED(HR)) {
                pFileOpen->Release();
                return {};
            }

            HR = pFileOpen->SetOptions(Options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
            if (FAILED(HR)) {
                pFileOpen->Release();
                return {};
            }

            if (exists(StartFolder)) { SetStartFolder(pFileOpen, StartFolder); }

            HR = pFileOpen->Show(Owner);
            if (FAILED(HR)) {
                pFileOpen->Release();
                return {};
            }

            IShellItem* pItem = nullptr;
            HR                = pFileOpen->GetResult(&pItem);
            if (FAILED(HR)) {
                pItem->Release();
                pFileOpen->Release();
                return {};
            }

            PWSTR FolderPath = nullptr;
            HR               = pItem->GetDisplayName(SIGDN_FILESYSPATH, &FolderPath);
            if (FAILED(HR)) {
                pItem->Release();
                pFileOpen->Release();
                return {};
            }

            std::filesystem::path OutPath = FolderPath;
            ::CoTaskMemFree(FolderPath);
            pItem->Release();
            pFileOpen->Release();

            return OutPath;
        }
    }  // namespace FileDialogs
}  // namespace Xen