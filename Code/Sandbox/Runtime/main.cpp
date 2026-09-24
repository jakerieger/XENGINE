//
// Created by Jake Rieger on 9/17/2026.
//

#include "Sandbox.hpp"

#include <Xen/XenGameSettings.h>

#ifndef NDEBUG
    #include "SceneBuilder.hpp"
#endif

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    Xen::FixContentWorkingDirectory();

    try {
        Xen::ProcessCommandLineArguments Arguments {};
        if (!Xen::GetProcessCommandLineArguments(Arguments)) {
            LOG_ERR("Failed to get command line arguments");
            return 1;
        }

        Sandbox Game("Sandbox", Xen::BuildMountConfig(Xen::Generated::GameSettings(), Arguments.Argc, Arguments.Argv));

#ifndef NDEBUG
        SceneBuilder::Build(Game.GetContext());
#endif

        Game.Run();
    } catch (...) { return 1; }

    return 0;
}
