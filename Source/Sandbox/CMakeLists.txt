project(Sandbox)

include(XenGame)
xen_add_game_executable(Sandbox
        Runtime/Res/App.rc
        Runtime/Res/Resource.h
        Runtime/main.cpp
        Runtime/RotatingComponent.cpp
        Runtime/RotatingComponent.hpp
        Runtime/Sandbox.cpp
        Runtime/Sandbox.hpp
        Runtime/SceneBuilder.hpp
)
xen_configure_game(Sandbox
        PAK_FILENAME "Data.pxk"
        CONTENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Content"
)
xen_compile_shaders(Sandbox)
xen_package_engine_content(Sandbox)
xen_package_game_content(Sandbox)

target_include_directories(Sandbox PRIVATE Runtime)
