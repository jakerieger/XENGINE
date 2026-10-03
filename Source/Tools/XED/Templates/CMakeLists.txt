project(${GAME_CLASS})

find_package(Xen REQUIRED)
include(XenGame)

xen_add_game_executable(${GAME_CLASS}
        Runtime/main.cpp
        Runtime/${GAME_CLASS}.cpp
        Runtime/${GAME_CLASS}.hpp
)
xen_configure_game(${GAME_CLASS}
        PAK_FILENAME "Data.pxk"
        CONTENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Content"
)
xen_package_game_content(${GAME_CLASS})
target_include_directories(${GAME_CLASS} PRIVATE Runtime)