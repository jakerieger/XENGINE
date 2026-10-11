cmake_minimum_required(VERSION 3.26)
project(${GAME_CLASS} CXX)

# Point CMake at the engine install with -DCMAKE_PREFIX_PATH=<engine install dir>.
# XED writes a CMakeUserPresets.json that does this for you when it runs from
# an installed engine.
find_package(Xen ${ENGINE_VERSION} REQUIRED)
include(XenGame)

xen_add_game_executable(${GAME_CLASS}
        Runtime/main.cpp
        Runtime/${GAME_CLASS}.cpp
        Runtime/${GAME_CLASS}.hpp
)
xen_configure_game(${GAME_CLASS}
        PAK_FILENAME "Data.pxk"
        CONTENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Content"
)
xen_package_engine_content(${GAME_CLASS})
xen_package_game_content(${GAME_CLASS})
target_include_directories(${GAME_CLASS} PRIVATE Runtime)

# ${GAME_CLASS}Module.dll: the same sources as a DLL for XED to load, so the
# editor knows this game's own Game class and components. Built on demand by
# XED (the 'editor' preset), not by a normal build.
xen_add_game_module(${GAME_CLASS})
