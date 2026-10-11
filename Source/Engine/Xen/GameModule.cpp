//
// Created by Jake Rieger on 10/3/2026.
//

#include "GameModule.hpp"

#include <Common/Log.hpp>

#include <Windows.h>

#include <format>

namespace Xen {
    namespace {
        ModuleID NextModuleID() {
            static ModuleID Next = NoModule;
            return ++Next;
        }

        std::string LastErrorString() {
            const DWORD Code = ::GetLastError();
            char* Buffer     = nullptr;
            const DWORD Len  = ::FormatMessageA(
              FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
              nullptr,
              Code,
              0,
              RCAST<LPSTR>(&Buffer),
              0,
              nullptr);
            std::string Message = Len ? std::string(Buffer, Len) : std::string();
            if (Buffer) ::LocalFree(Buffer);
            while (!Message.empty() && (Message.back() == '\n' || Message.back() == '\r' || Message.back() == ' ')) {
                Message.pop_back();
            }
            return std::format("{} (error {})", Message, Code);
        }
    }  // namespace

    GameModule::~GameModule() {
        Unload();
    }

    bool GameModule::Load(const fs::path& DllPath, std::string& Error) {
        Unload();

        if (!exists(DllPath)) {
            Error = std::format("module not found: '{}'", DllPath.string());
            return false;
        }

        // The module's XEN_COMPONENT registrars run inside LoadLibrary, so the
        // registry scope has to be open around the load itself.
        const ModuleID ID = NextModuleID();
        auto& Registry    = ComponentRegistry::Get();

        Registry.BeginModuleScope(ID);
        // Resolve the module's own dependencies from its directory too, not
        // just the host's - Xen.dll itself is already loaded by name.
        const HMODULE Handle = ::LoadLibraryExW(DllPath.c_str(),
                                                nullptr,
                                                LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        const std::vector<std::string> RegistrationErrors = Registry.EndModuleScope();

        if (!Handle) {
            const DWORD Code = ::GetLastError();
            Registry.UnregisterModule(ID);
            Error = std::format("failed to load module '{}': {}", DllPath.string(), LastErrorString());
            // ERROR_MOD_NOT_FOUND for a module that *exists* means one of its
            // own imports is missing - typically the engine DLL of the other
            // runtime flavour (Xend.dll for a Debug-built module in a Release
            // host, or the reverse).
            if (Code == ERROR_MOD_NOT_FOUND) {
                Error += " - one of its dependencies is missing; most likely it was built against the other "
                         "engine runtime (Xen.dll vs Xend.dll). Rebuild it in a configuration matching the host.";
            }
            return false;
        }

        const auto Reject = [&](std::string Reason) {
            Registry.UnregisterModule(ID);
            ::FreeLibrary(Handle);
            Error = std::format("module '{}' rejected: {}", DllPath.string(), Reason);
            return false;
        };

        if (!RegistrationErrors.empty()) {
            std::string Joined;
            for (const auto& E : RegistrationErrors) Joined += "\n  " + E;
            return Reject("component registration failed:" + Joined);
        }

        const auto GetInfo = RCAST<ModuleGetInfoFn>(::GetProcAddress(Handle, ModuleExports::GetInfo));
        const auto Create  = RCAST<ModuleCreateGameFn>(::GetProcAddress(Handle, ModuleExports::CreateGame));
        const auto Destroy = RCAST<ModuleDestroyGameFn>(::GetProcAddress(Handle, ModuleExports::DestroyGame));
        if (!GetInfo || !Create || !Destroy) {
            return Reject("missing module exports - is it a game module (XEN_GAME / XEN_GAME_MODULE)?");
        }

        const ModuleInfo* Info = GetInfo();
        if (!Info) return Reject("XenModule_GetInfo returned null");
        if (Info->AbiVersion != XEN_MODULE_ABI_VERSION) {
            return Reject(
              std::format("module ABI version {}, host expects {}", Info->AbiVersion, XEN_MODULE_ABI_VERSION));
        }
        if (!Info->EngineVersion || std::string_view(Info->EngineVersion) != XEN_ENGINE_VERSION) {
            return Reject(std::format("built against engine {}, host is {}",
                                      Info->EngineVersion ? Info->EngineVersion : "(none)",
                                      XEN_ENGINE_VERSION));
        }
        if (Info->DebugRuntime != XEN_MODULE_DEBUG_RUNTIME || Info->IteratorDebugLevel != _ITERATOR_DEBUG_LEVEL) {
            return Reject(std::format("built with the {} C++ runtime, host uses the {} one - rebuild the module in "
                                      "a configuration matching the host (e.g. the 'editor' preset for XED)",
                                      Info->DebugRuntime ? "Debug" : "Release",
                                      XEN_MODULE_DEBUG_RUNTIME ? "Debug" : "Release"));
        }

        _Handle      = Handle;
        _ID          = ID;
        _Info        = *Info;
        _Path        = DllPath;
        _CreateGame  = Create;
        _DestroyGame = Destroy;

        LOG_INFO("Loaded game module '%s' (%s) - %zu component type(s)",
                 _Info.GameName ? _Info.GameName : "?",
                 DllPath.filename().string().c_str(),
                 Registry.GetModuleTypeCount(_ID));
        return true;
    }

    void GameModule::Unload() {
        if (!_Handle) return;

        const size_t Removed = ComponentRegistry::Get().UnregisterModule(_ID);
        ::FreeLibrary(CAST<HMODULE>(_Handle));
        LOG_INFO("Unloaded game module '%s' (%zu component type(s) unregistered)",
                 _Path.filename().string().c_str(),
                 Removed);

        _Handle      = nullptr;
        _ID          = NoModule;
        _Info        = ModuleInfo {};
        _Path.clear();
        _CreateGame  = nullptr;
        _DestroyGame = nullptr;
    }

    Game* GameModule::CreateGame(const EmbeddedGameArgs& Args) const {
        if (!_CreateGame) {
            THROW_ENGINE_EXCEPTION(EngineException, "GameModule::CreateGame called with no module loaded");
        }
        return _CreateGame(Args);
    }

    void GameModule::DestroyGame(Game* G) const {
        if (G && _DestroyGame) _DestroyGame(G);
    }
}  // namespace Xen
