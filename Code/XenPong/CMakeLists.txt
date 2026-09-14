project(Xen2D)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/XenPong)

add_executable(XenPong
        main.cpp
        BallComponent.cpp
        BallComponent.hpp
        OpponentComponent.cpp
        OpponentComponent.hpp
        PlayerComponent.cpp
        PlayerComponent.hpp
)

target_include_directories(XenPong PRIVATE ${MODULES_ROOT} ${MODULES_ROOT}/XenPAK ${VENDOR_ROOT})

xen_configure_game(XenPong
        PAK_FILENAME "Data1.xpak"
        CONTENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Content"
)

add_custom_command(
        TARGET XenPong POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Config"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Config"
)