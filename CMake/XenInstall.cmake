# Install rules and the find_package(Xen) package config.
#
# Debug and Release are meant to be installed into the same prefix (the
# libraries carry a 'd' Debug postfix - see the top-level CMakeLists.txt):
#
#   cmake --install build/Debug   --prefix <prefix>
#   cmake --install build/Release --prefix <prefix>
#
# A game then only needs -DCMAKE_PREFIX_PATH=<prefix> and find_package(Xen).
#
# Layout:
#   bin/                    Xen[d].dll (+ .pdb), tools (Release builds only)
#   XED/                    the editor: Bin64/XED.exe, Config/, EngineContent/, Templates/ (Release only)
#   include/                Xen/ Common/ XenPAK/ (public engine headers), D3D12MemAlloc.h, directx/
#   include/XenVendor/      vendored headers games use: imgui, nlohmann/json, ini.h
#   lib/                    Xen[d].lib import libs, dependency static libs (Debug ones end in 'd')
#   share/Xen/cmake/        XenConfig.cmake and friends, XenGame.cmake (game helpers)
#   share/Xen/EngineContent/<Config>/  prebuilt engine paks (XEN.Shaders.pxk, XEN.Environment.pxk)
#   share/Xen/.pakignore    default ignore list for packing game content
#   share/directx-headers/, share/cmake/D3D12MemoryAllocator/ - those deps' own packages

include(CMakePackageConfigHelpers)

set(XEN_INSTALL_DATADIR "${CMAKE_INSTALL_DATAROOTDIR}/Xen")
set(XEN_INSTALL_CMAKEDIR "${XEN_INSTALL_DATADIR}/cmake")

# The engine is a single DLL - Common, PAK and lz4 are compiled into it, so
# Xen::Xen is the only engine target a game ever sees.
install(TARGETS Xen
        EXPORT XenTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILE_SET vendor DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/XenVendor
        FILE_SET imgui DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/XenVendor
)
install(EXPORT XenTargets
        NAMESPACE Xen::
        DESTINATION ${XEN_INSTALL_CMAKEDIR}
)
# The DLL's debug symbols, so a game's debugger can step into the engine.
install(FILES $<TARGET_PDB_FILE:Xen> DESTINATION ${CMAKE_INSTALL_BINDIR} OPTIONAL)

# Tools are installed from Release builds only - otherwise the Debug and
# Release installs would overwrite each other's bin/*.exe. PAKTool is exported
# (as Xen::PAKTool) because game content packaging runs it.
if (TARGET PAKTool)
    install(TARGETS PAKTool
            EXPORT XenToolsTargets
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} CONFIGURATIONS Release
    )
    install(EXPORT XenToolsTargets
            NAMESPACE Xen::
            DESTINATION ${XEN_INSTALL_CMAKEDIR}
            CONFIGURATIONS Release
    )
endif ()
if (TARGET Bin2CC)
    install(TARGETS Bin2CC RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} CONFIGURATIONS Release)
endif ()

# XED keeps the same layout as any Xen game (Game::FixContentWorkingDirectory
# expects Config/, EngineContent/ etc. one level above Bin64/), plus its
# new-project Templates/. Release only, like the other tools. An installed XED
# finds its own engine install from there (two levels above XED.exe) and
# points new projects' CMakeUserPresets.json at it.
if (TARGET XED)
    set(_xed_dir "${TOOLS_ROOT}/XED")
    install(TARGETS XED RUNTIME DESTINATION XED/Bin64 CONFIGURATIONS Release)
    install(FILES $<TARGET_FILE:Xen> DESTINATION XED/Bin64 CONFIGURATIONS Release)
    install(DIRECTORY "${_xed_dir}/Config/" DESTINATION XED/Config CONFIGURATIONS Release)
    install(DIRECTORY "${_xed_dir}/Templates/" DESTINATION XED/Templates CONFIGURATIONS Release
            PATTERN ".clang-format" EXCLUDE)
    install(DIRECTORY "${XEN_ENGINE_PAK_DIR}/" DESTINATION XED/EngineContent CONFIGURATIONS Release)
    unset(_xed_dir)
endif ()

# Game helpers (include(XenGame) - XenConfig.cmake puts this dir on
# CMAKE_MODULE_PATH) and the engine's default pak ignore list.
install(FILES
        ${SOURCE_ROOT}/Modules/Xen/CMake/XenGame.cmake
        ${SOURCE_ROOT}/Modules/Xen/CMake/XenGameSettings.h.in
        DESTINATION ${XEN_INSTALL_CMAKEDIR}
)
install(FILES ${CMAKE_SOURCE_DIR}/.pakignore DESTINATION ${XEN_INSTALL_DATADIR})

# Prebuilt engine paks, one variant per installed config (EngineContent/Debug,
# EngineContent/Release) - XenConfig.cmake picks the right one per game config.
if (TARGET xen_engine_paks)
    install(DIRECTORY "${XEN_ENGINE_PAK_DIR}/"
            DESTINATION "${XEN_INSTALL_DATADIR}/EngineContent/$<CONFIG>"
    )
endif ()

configure_package_config_file(
        ${CMAKE_CURRENT_LIST_DIR}/XenConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/XenConfig.cmake
        INSTALL_DESTINATION ${XEN_INSTALL_CMAKEDIR}
        PATH_VARS XEN_INSTALL_DATADIR
)
# Pre-1.0, a minor version bump may break the API, so only an exact
# major.minor match is compatible.
write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/XenConfigVersion.cmake
        COMPATIBILITY SameMinorVersion
)
install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/XenConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/XenConfigVersion.cmake
        DESTINATION ${XEN_INSTALL_CMAKEDIR}
)
