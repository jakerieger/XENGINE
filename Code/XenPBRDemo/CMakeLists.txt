project(XenPBRDemo)

include(XenGame)
xen_add_game_executable(XenPBRDemo
        Runtime/main.cpp
        Runtime/Source/RotatingComponent.cpp
        Runtime/Source/RotatingComponent.hpp
)
xen_configure_game(XenPBRDemo
        PAK_FILENAME "Data1.xpak"
        CONTENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Content"
)
xen_compile_shaders(XenPBRDemo)
xen_package_game_content(XenPBRDemo)

target_include_directories(XenPBRDemo PRIVATE Runtime Runtime/Source ${MODULES_ROOT} ${VENDOR_ROOT})
