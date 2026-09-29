cmake_minimum_required(VERSION 3.14..)
project(XENGINE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
set(MODULES_ROOT ${SOURCE_ROOT}/Modules)
set(VENDOR_ROOT ${SOURCE_ROOT}/Vendor)
set(TOOLS_ROOT ${SOURCE_ROOT}/Tools)

# 3rd party dependencies
include(CMake/FetchDeps.cmake)
include(Source/Vendor/Vendor.cmake)

set(TOOLS_BIN_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Tools")
message(STATUS "Tools bin directory: ${TOOLS_BIN_DIR}")

# CMake helpers for engine
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Source/Modules/Xen/CMake")

add_subdirectory(${MODULES_ROOT}/Common)
add_subdirectory(${MODULES_ROOT}/Xen)
add_subdirectory(${MODULES_ROOT}/XenPAK)
add_subdirectory(${MODULES_ROOT}/CrashHandler)

if (BUILD_TOOLS)
    add_subdirectory(${TOOLS_ROOT})
endif ()

if (BUILD_SANDBOX)
    add_subdirectory(${SOURCE_ROOT}/Sandbox)
endif ()