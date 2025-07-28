cmake_minimum_required(VERSION 3.14)
project(BallRoller)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(RUNTIME_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(XENGINE REQUIRED)

include_directories(
	${XENGINE_IMGUI_INC} # Needed because ImGui uses `#include <imgui.h>` everywhere
)

add_executable(BallRoller
	${RUNTIME_DIR}/main.cpp
)

target_link_libraries(BallRoller PRIVATE X::x X::xpak)