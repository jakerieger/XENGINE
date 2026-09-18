project(Xen2D)

include(XenGame)
xen_add_game_executable(XenPBRDemo
        main.cpp
        RotatingComponent.cpp
        RotatingComponent.hpp
)

xen_configure_game(XenPBRDemo
        PAK_FILENAME "Data1.xpak"
        CONTENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Content"
)

target_include_directories(XenPBRDemo PRIVATE ${MODULES_ROOT} ${VENDOR_ROOT})

# Copy `Config` directory to build directory since it's needed at runtime.
# $<TARGET_FILE_DIR:XenPBRDemo> (not CMAKE_RUNTIME_OUTPUT_DIRECTORY) because
# the per-config output dirs the top-level CMakeLists.txt sets take
# precedence over the generic variable this file reassigned above, on a
# multi-config generator - using the variable directly copied Config next to
# a folder the .exe was never actually placed in.
add_custom_command(
        TARGET XenPBRDemo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Config"
        "$<TARGET_FILE_DIR:XenPBRDemo>/Config"
)
