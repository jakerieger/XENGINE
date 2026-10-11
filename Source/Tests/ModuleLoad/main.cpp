//
// Created by Jake Rieger on 10/3/2026.
//
// Headless test for game module loading (Xen/GameModule.hpp). Loads the
// in-tree SandboxModule.dll and checks the loader, the registry's module
// ownership, reload cycles, bad inputs, and placeholder round-tripping of
// components whose module isn't loaded.
//
// Needs no window or render device, so it can't cover CreateGame (that takes
// a live RHI device) - XED itself exercises that path.
//
// Exit code 0 = all checks passed.

#include "BuildProcess.hpp"

#include <Xen/ComponentRegistry.hpp>
#include <Xen/GameModule.hpp>
#include <Xen/PlaceholderComponent.hpp>
#include <Xen/Scene.hpp>
#include <Xen/SceneSerializer.hpp>

#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>
#include <string>

#ifndef XEN_TEST_MODULE_PATH
    #error "XEN_TEST_MODULE_PATH must be defined (the game module to load)"
#endif
#ifndef XEN_TEST_NOT_A_MODULE_PATH
    #error "XEN_TEST_NOT_A_MODULE_PATH must be defined (a DLL that isn't a game module)"
#endif

using namespace Xen;

namespace {
    int Failures = 0;
    int Checks   = 0;

    void Check(const bool Condition, const char* What, const int Line) {
        ++Checks;
        if (Condition) {
            std::printf("  ok    %s\n", What);
        } else {
            ++Failures;
            std::printf("  FAIL  %s (line %d)\n", What, Line);
        }
    }
#define CHECK(Cond) Check((Cond), #Cond, __LINE__)

    constexpr const char* ModuleComponent = "RotatingComponent";  // defined by Sandbox, not the engine

    bool Registered() {
        return ComponentRegistry::Get().IsRegistered(std::string(ModuleComponent));
    }

    bool Contains(const std::string& Haystack, const char* Needle) {
        return Haystack.find(Needle) != std::string::npos;
    }

    // A scene with one actor carrying one module component, as JSON. Built and
    // destroyed here so no component outlives the module that made it.
    std::string BuildSceneWithModuleComponent() {
        Scene S("Test");
        const auto Handle = S.Spawn("Spinner");
        Actor* A          = S.Get(Handle);
        A->AdoptComponent(ComponentRegistry::Get().Create(std::string(ModuleComponent)));
        return SceneSerializer::SaveToString(S);
    }

    void TestBadInputs() {
        std::printf("bad inputs\n");
        GameModule Module;
        std::string Error;
        const size_t TypesBefore = ComponentRegistry::Get().GetTypeCount();

        CHECK(!Module.Load("this/does/not/exist.dll", Error));
        CHECK(Contains(Error, "not found"));
        CHECK(!Module.IsLoaded());

        // A real DLL with none of the module exports.
        Error.clear();
        CHECK(!Module.Load(XEN_TEST_NOT_A_MODULE_PATH, Error));
        CHECK(Contains(Error, "missing module exports"));
        CHECK(!Module.IsLoaded());

        // Rejected loads leave nothing behind.
        CHECK(ComponentRegistry::Get().GetTypeCount() == TypesBefore);
        CHECK(!Registered());

        // Unload with nothing loaded is a no-op.
        Module.Unload();
        CHECK(!Module.IsLoaded());
    }

    void TestLoadUnloadReload() {
        std::printf("load / unload / reload\n");
        auto& Registry = ComponentRegistry::Get();
        const size_t EngineTypes = Registry.GetTypeCount();
        CHECK(!Registered());

        GameModule Module;
        std::string Error;

        for (int Cycle = 1; Cycle <= 3; ++Cycle) {
            std::printf(" cycle %d\n", Cycle);
            CHECK(Module.Load(XEN_TEST_MODULE_PATH, Error));
            if (!Module.IsLoaded()) {
                std::printf("  load error: %s\n", Error.c_str());
                return;
            }

            const ModuleInfo& Info = Module.GetInfo();
            CHECK(Info.AbiVersion == XEN_MODULE_ABI_VERSION);
            CHECK(Info.GameName && std::string(Info.GameName) == "Sandbox");
            CHECK(Info.EngineVersion && std::string(Info.EngineVersion) == XEN_ENGINE_VERSION);
            CHECK(Module.GetID() != NoModule);

            // Registrations that ran during LoadLibrary were tagged with this module.
            CHECK(Registered());
            CHECK(Registry.GetModuleTypeCount(Module.GetID()) >= 1);
            const auto* Type = Registry.FindType(Hash::FNV1A(ModuleComponent, std::string(ModuleComponent).size()));
            CHECK(Type && Type->Owner == Module.GetID());
            CHECK(Registry.Create(std::string(ModuleComponent)) != nullptr);

            // Engine types stay untagged.
            CHECK(Registry.GetTypeCount() > EngineTypes);

            Module.Unload();
            CHECK(!Module.IsLoaded());
            CHECK(!Registered());
            CHECK(Registry.Create(std::string(ModuleComponent)) == nullptr);
            CHECK(Registry.GetTypeCount() == EngineTypes);
        }

        // Loading over a loaded module replaces it.
        CHECK(Module.Load(XEN_TEST_MODULE_PATH, Error));
        const ModuleID First = Module.GetID();
        CHECK(Module.Load(XEN_TEST_MODULE_PATH, Error));
        CHECK(Module.GetID() != First);
        CHECK(Registry.GetModuleTypeCount(First) == 0);
        CHECK(Registered());
    }

    void TestPlaceholders() {
        std::printf("placeholder round trip\n");
        GameModule Module;
        std::string Error;

        CHECK(Module.Load(XEN_TEST_MODULE_PATH, Error));
        const std::string WithModule = BuildSceneWithModuleComponent();
        CHECK(Contains(WithModule, ModuleComponent));

        Module.Unload();
        CHECK(!Registered());

        // Without the module the component loads as a placeholder and saves back unchanged.
        std::string Resaved;
        {
            Scene S("Test");
            SceneSerializer::LoadFromString(S, WithModule);

            Actor* A = nullptr;
            S.ForEachActor([&A](const Actor& Found) { A = const_cast<Actor*>(&Found); });
            CHECK(A && A->GetComponentCount() == 1);
            if (A && A->GetComponentCount() == 1) {
                const auto* P = dynamic_cast<const PlaceholderComponent*>(A->GetComponentAt(0));
                CHECK(P != nullptr);
                CHECK(P && std::string(P->GetTypeName()) == ModuleComponent);
            }
            Resaved = SceneSerializer::SaveToString(S);
        }
        CHECK(Resaved == WithModule);

        // With the module loaded again, the same data becomes the real component.
        CHECK(Module.Load(XEN_TEST_MODULE_PATH, Error));
        {
            Scene S("Test");
            SceneSerializer::LoadFromString(S, Resaved);

            Actor* A = nullptr;
            S.ForEachActor([&A](const Actor& Found) { A = const_cast<Actor*>(&Found); });
            CHECK(A && A->GetComponentCount() == 1);
            if (A && A->GetComponentCount() == 1) {
                CHECK(dynamic_cast<const PlaceholderComponent*>(A->GetComponentAt(0)) == nullptr);
                CHECK(std::string(A->GetComponentAt(0)->GetTypeName()) == ModuleComponent);
            }
        }  // scene (and its module component) destroyed before the module unloads
        Module.Unload();
    }

    void RunProcess(const wchar_t* Command, std::vector<std::string>& Lines, int& ExitCode, std::string& Error) {
        BuildProcess P;
        if (!P.Start(Command, fs::current_path(), Error)) { ExitCode = -1; return; }
        while (!P.Poll(Lines, ExitCode)) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
    }

    void TestBuildProcess() {
        std::printf("build process\n");
        std::vector<std::string> Lines;
        int Code = -1;
        std::string Error;

        // stdout and stderr both arrive, line by line, and the exit code is reported.
        RunProcess(L"cmd.exe /c \"echo first& echo second>&2& exit /b 3\"", Lines, Code, Error);
        CHECK(Error.empty());
        CHECK(Code == 3);
        CHECK(Lines.size() == 2);
        CHECK(Lines.size() == 2 && Lines[0] == "first" && Lines[1] == "second");

        Lines.clear();
        RunProcess(L"cmd.exe /c exit /b 0", Lines, Code, Error);
        CHECK(Code == 0 && Lines.empty());

        // A command that can't be started is an error, not a hang.
        Lines.clear();
        RunProcess(L"definitely-not-a-program.exe", Lines, Code, Error);
        CHECK(!Error.empty());
    }
}  // namespace

int main() {
    TestBuildProcess();
    TestBadInputs();
    TestLoadUnloadReload();
    TestPlaceholders();

    std::printf("\n%d checks, %d failed\n", Checks, Failures);
    return Failures == 0 ? 0 : 1;
}
