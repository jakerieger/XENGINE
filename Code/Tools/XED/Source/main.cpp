//
// Created by Jake Rieger on 9/27/2026.
//

#include <Common/Platform.hpp>

#include "Xed.hpp"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    // Same reasoning as RunGame's own call (Game.hpp): every relative path
    // this process resolves (Config/, EngineContent/, Data.pxk) assumes the
    // working directory is the parent of wherever the .exe actually is, not
    // the .exe's own Bin64 directory a normal launch defaults to.
    Xen::FixContentWorkingDirectory();

#ifndef NDEBUG
    Xen::AttachConsole("XED");
#endif

    try {
        Xen::XED Editor;
        Editor.Run();
    } catch (const Xen::EngineException& Ex) {
        std::fprintf(stderr, "%s\n", Ex.what());
        return 1;
    }

    return 0;
}