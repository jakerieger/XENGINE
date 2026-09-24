project(Sandbox)

include(XenGame)
xen_add_game_executable(Sandbox
        Runtime/Res/App.rc
        Runtime/Res/Resource.h
        Runtime/main.cpp
        Runtime/Source/RotatingComponent.cpp
        Runtime/Source/RotatingComponent.hpp
        Runtime/Source/Sandbox.cpp
        Runtime/Source/Sandbox.hpp
        Runtime/Source/SceneBuilder.hpp
)
xen_configure_game(Sandbox
        PAK_FILENAME "Data.pxk"
        CONTENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Content"
)
xen_compile_shaders(Sandbox)
xen_package_game_content(Sandbox)

target_include_directories(Sandbox PRIVATE Runtime Runtime/Source ${MODULES_ROOT} ${VENDOR_ROOT})
