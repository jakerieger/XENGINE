# Development Log

## Contents

**February 2025:**

- [Feb 20, 2025](#feb-20-2025)
- [Feb 19, 2025](#feb-19-2025)
- [Feb 18, 2025](#feb-18-2025)
- [Feb 17, 2025](#feb-17-2025)

---

## Feb 20, 2025

I've successfully decoupled the game engine core from the windowing system. I started working on the
scene editor which at the moment draws some empty panels and the scene view in the middle. Scene
files can be opened and are displayed in the scene view; pause and resume works for game updates.

The biggest problem at the moment is resizing for the game viewport isn't working or responding
correctly. This is likely due to the resizing methods used within the engine core and will likely
need to be re-worked. The [Viewport](../Code/Engine/Viewport.hpp) class proved successful in its
design goals during testing.

The main focus now is to fix all the bugs with the embedded core inside an independently sized
viewport not directly associated with the window size or resize events. Input is another problem to
tackle, as editor input should be separate from game input.

Some other considerations are handling scene viewport focus events, also related to input
management. Game input should only be registered when the viewport is focused, otherwise input is
registered for the editor itself.

> Commit: [318f5bc](https://github.com/jakerieger/SpaceGame/commit/318f5bc483d2a633cddade8259c3de66299b3012)

### New classes:

- [EditorWindow](../Code/Editor/EditorWindow.hpp)
- [FileDialogs](../Code/Editor/FileDialogs.hpp)

## Feb 19, 2025

Began working on a new system for decoupling the game core from the window system. While the latest
commit to the `window` branch
is working, some things are notably missing. I still need to test whether I can embed the
core into
a viewport that doesn't take up the entire window, although
the [Viewport](../Code/Engine/Viewport.hpp) abstraction should
allow this.

### New classes:

- [Viewport](../Code/Engine/Viewport.hpp)
- [Window](../Code/Engine/Window.hpp)
- [Event](../Code/Engine/Event.hpp)
- [EventListener](../Code/Engine/EventListener.hpp)
- [EventEmitter](../Code/Engine/EventEmitter.hpp)

The [Game](../Code/Engine/Game.hpp) class has been **heavily** refactored to support this
new change and is
entirely incompatible with the old system. Once everything has been thoroughly tested, the `window`
branch will be merged with `master`.

> **TODO**: Update documentation to reflect these changes.

## Feb 18, 2025

Transition to CMake build system has been completed and merged into `master`. Descriptor files for
scenes and materials have been converted from JSON to YAML for simpler editing and parsing. Scene
loading and descriptor parsing has obviously been reworked due to the change in format, but also for
a better designed system overall.

All third party dependencies are now fetched automatically by the build system; no need for git
submodules. Hence, they have been removed. Everything else remains consistent with previous versions
of the project.

Continuing work on seperating the engine core from the windowing system so work on a level editor
can begin. This will likely be the following week(s) focus.

## Feb 17, 2025

I'm thinking about migrating this entire project to a Premake build system. This would simplify a
lot of things and make it so that while development can continue in VS 2022, project structure isn't
defined by the way VS likes to do things (something I'm *not* a fan of).

> Cross-platform compatibility is not a consideration, so this change would be exclusively for
> developer convenience.

I'd project this would take a couple hours worth of work. A new branch will be created for this
migration and if successful, merged back with the master branch.

Some other design considerations are the game framework itself. As of right now, I have no way of
embedding the actual engine core in an editor, although I'm not too far off. The current Game class
would need to be split into separate components:

- One for the actual engine
- One for the window management system (Win32)

This way, the game can be embedded in whichever window system it needs to be, regardless of whether
the game owns that window or not. Nothing else should need to be refactored.

I'm also not sold on using JSON for descriptor files, XML feels like a better fit. This would
require reworking a few classes but overall would not be much of a detour. Ideally, custom binary
file formats would be designed specifically for whatever use case they need to be used for, but this
is not a priority or something on the docket for the near future.
