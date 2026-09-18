project(Demo.PBR)

include(XenGame)
xen_add_game_executable(Demo.PBR
        Runtime/main.cpp
        Runtime/Source/RotatingComponent.cpp
        Runtime/Source/RotatingComponent.hpp
)
xen_configure_game(Demo.PBR
        PAK_FILENAME "Data1.xpak"
        CONTENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Content"
)
xen_compile_shaders(Demo.PBR)
xen_package_game_content(Demo.PBR)

target_include_directories(Demo.PBR PRIVATE Runtime Runtime/Source ${MODULES_ROOT} ${VENDOR_ROOT})
