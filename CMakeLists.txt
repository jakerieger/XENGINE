cmake_minimum_required(VERSION 3.14..)
project(Xen2D)

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

# 3rd party dependencies
include(CMake/FetchDeps.cmake)

set(CODE_ROOT ${CMAKE_SOURCE_DIR}/Code)
set(MODULES_ROOT ${CODE_ROOT}/Modules)
set(VENDOR_ROOT ${CODE_ROOT}/Vendor)
set(TOOLS_ROOT ${CODE_ROOT}/Tools)

set(TOOLS_BIN_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Tools")
message(STATUS "Tools bin directory: ${TOOLS_BIN_DIR}")

# CMake helpers for engine
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Code/Modules/Xen/CMake")

add_subdirectory(${MODULES_ROOT}/Common)
add_subdirectory(${MODULES_ROOT}/Xen)
add_subdirectory(${MODULES_ROOT}/XenPAK)
add_subdirectory(${MODULES_ROOT}/CrashHandler)
add_subdirectory(${TOOLS_ROOT})

# Demo projects
add_subdirectory(${CODE_ROOT}/XenPong)
add_subdirectory(${CODE_ROOT}/XenPBRDemo)