project(XENGINE)

include(XenGame)
xen_add_game_executable(Demo.Pong
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
xen_configure_game(Demo.Pong
        PAK_FILENAME "Data.pxk"
        CONTENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Content"
)
xen_compile_shaders(Demo.Pong)
xen_package_game_content(Demo.Pong)

target_include_directories(Demo.Pong PRIVATE Runtime Runtime/Source ${MODULES_ROOT} ${VENDOR_ROOT})