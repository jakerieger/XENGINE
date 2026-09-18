project(Xen2D)

include(XenGame)
xen_add_game_executable(XenPong
        Runtime/main.cpp
        Runtime/Source/BallComponent.cpp
        Runtime/Source/BallComponent.hpp
        Runtime/Source/GameManagerComponent.cpp
        Runtime/Source/GameManagerComponent.hpp
        Runtime/Source/OpponentComponent.cpp
        Runtime/Source/OpponentComponent.hpp
        Runtime/Source/PlayerComponent.cpp
        Runtime/Source/PlayerComponent.hpp
)
xen_configure_game(XenPong
        PAK_FILENAME "Data1.xpak"
        CONTENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Content"
)
xen_compile_shaders(XenPong)
xen_package_game_content(XenPong)

target_include_directories(XenPong PRIVATE Runtime Runtime/Source ${MODULES_ROOT} ${VENDOR_ROOT})