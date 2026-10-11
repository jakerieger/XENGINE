cmake_minimum_required(VERSION 3.26)
project(XENGINE VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

# Debug and Release are installed side by side into one prefix (MSVC can't mix
# the two runtimes in one link), so every library - ours and the fetched deps
# alike, hence before FetchDeps - needs a distinct Debug file name.
# Executables ignore this variable.
set(CMAKE_DEBUG_POSTFIX d)

# Embed debug info in the .obj/.lib files (/Z7) instead of per-target compile
# PDBs (/Zi): the installed static libs then carry their own debug info, rather
# than every game's Debug link warning LNK4099 about PDBs that were never
# installed. Linked executables still produce their own PDB as usual.
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

foreach (OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/lib)
endforeach ()

option(BUILD_TOOLS "Build XENGINE tools (editor, paktool, etc.)" ON)
option(BUILD_SANDBOX "Build XENGINE Sandbox game" ON)

set(SOURCE_ROOT ${CMAKE_SOURCE_DIR}/Source)
set(ENGINE_ROOT ${SOURCE_ROOT}/Engine)
set(VENDOR_ROOT ${SOURCE_ROOT}/Vendor)
set(TOOLS_ROOT ${SOURCE_ROOT}/Tools)

# 3rd party dependencies
include(CMake/FetchDeps.cmake)
include(Source/Vendor/Vendor.cmake)

# CMake helpers for games (include(XenGame)). XenGame.cmake reads the
# variables below; an installed engine's XenConfig.cmake defines the same ones
# (minus the hot-reload paths), so it works in-tree and out-of-tree alike.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Source/Engine/Xen/CMake")
set(XEN_ENGINE_PAK_DIR "${CMAKE_BINARY_DIR}/EngineContent/$<CONFIG>")  # built by CMake/EngineContent.cmake
set(XEN_PAKIGNORE "${CMAKE_SOURCE_DIR}/.pakignore")
# In-tree only: Debug games hot-reload engine shaders straight from the source tree.
set(XEN_ENGINE_SHADER_SOURCE_DIR "${SOURCE_ROOT}/Shaders")
set(XEN_ENGINE_SHADER_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/EngineContent/Shaders")

# Common and XenPAK first: Xen (the engine DLL) compiles them in and reads
# their header sets at configure time.
add_subdirectory(${ENGINE_ROOT}/Common)
add_subdirectory(${ENGINE_ROOT}/XenPAK)
add_subdirectory(${ENGINE_ROOT}/Xen)
add_subdirectory(${ENGINE_ROOT}/CrashHandler)

# Engine shaders + engine paks. Before any game is added - see the file's header.
include(CMake/EngineContent.cmake)

if (BUILD_TOOLS)
    add_subdirectory(${TOOLS_ROOT})
endif ()

# Headless physics tests: no window, device or Sandbox needed.
add_subdirectory(${SOURCE_ROOT}/Tests/Physics)
add_subdirectory(${SOURCE_ROOT}/Tests/Prefab)

if (BUILD_SANDBOX)
    add_subdirectory(${SOURCE_ROOT}/Sandbox)
    add_subdirectory(${SOURCE_ROOT}/Tests/ModuleLoad)  # loads SandboxModule.dll
endif ()

# cmake --install rules + the find_package(Xen) package
include(CMake/XenInstall.cmake)