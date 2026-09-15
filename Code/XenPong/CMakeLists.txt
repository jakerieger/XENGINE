project(Xen2D)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/XenPong)

include(XenGame)
xen_add_game_executable(XenPong
        main.cpp
        BallComponent.cpp
        BallComponent.hpp
        OpponentComponent.cpp
        OpponentComponent.hpp
        PlayerComponent.cpp
        PlayerComponent.hpp
)

xen_configure_game(XenPong
        PAK_FILENAME "Data1.xpak"
        CONTENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Content"
)

target_include_directories(XenPong PRIVATE ${MODULES_ROOT} ${VENDOR_ROOT})

# Copy `Config` directory to build directory since it's needed at runtime
add_custom_command(
        TARGET XenPong POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Config"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Config"
)