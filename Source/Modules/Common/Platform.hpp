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

    namespace FileDialogs {
        struct FileTypeFilter {
            const wchar_t* Name;
            const wchar_t* Extensions;
        };

        inline std::optional<std::filesystem::path>
        OpenFileDialog(const HWND Owner, const std::wstring& Title, const std::vector<FileTypeFilter>& FileFilters) {
            std::filesystem::path OutPath;

            IFileOpenDialog* pFileOpen = nullptr;
            auto HR                    = ::CoCreateInstance(CLSID_FileOpenDialog,
                                         nullptr,
                                         CLSCTX_ALL,
                                         IID_IFileOpenDialog,
                                         reinterpret_cast<void**>(&pFileOpen));

            if (SUCCEEDED(HR)) {
                std::vector<COMDLG_FILTERSPEC> FileTypes;
                for (const auto& [Name, Extensions] : FileFilters) {
                    FileTypes.push_back({
                      .pszName = Name,
                      .pszSpec = Extensions,
                    });
                }
                HR = pFileOpen->SetFileTypes(static_cast<UINT>(FileTypes.size()), FileTypes.data());
                if (FAILED(HR)) return {};
                HR = pFileOpen->SetTitle(Title.c_str());
                if (FAILED(HR)) return {};

                HR = pFileOpen->Show(Owner);

                if (SUCCEEDED(HR)) {
                    IShellItem* pItem;
                    HR = pFileOpen->GetResult(&pItem);
                    if (SUCCEEDED(HR)) {
                        PWSTR FilePath;
                        HR = pItem->GetDisplayName(SIGDN_FILESYSPATH, &FilePath);

                        if (SUCCEEDED(HR)) {
                            OutPath = std::wstring(FilePath);
                            ::CoTaskMemFree(FilePath);
                        }

                        pItem->Release();
                    }
                }

                pFileOpen->Release();
            }

            return OutPath;
        }
    }  // namespace FileDialogs
}  // namespace Xen