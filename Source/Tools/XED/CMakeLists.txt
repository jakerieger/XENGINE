project(XENGINE)

include(XenGame)

file(GLOB_RECURSE IMGUI_SOURCES
        ${VENDOR_ROOT}/imgui/*.h
        ${VENDOR_ROOT}/imgui/*.cpp
)

file(GLOB RESOURCE_HEADERS
        Source/Resource/*.h
)

# xen_add_game_executable (not a raw add_executable), so XED's output lands
# in .../XED/Bin64 like every other Xen game - Game::FixContentWorkingDirectory
# (called from main.cpp, same as RunGame does for every other game) assumes
# exactly that layout (Config/, EngineContent/, Data.pxk one level above the
# .exe's own directory); the plain "${TARGET}" output dir XED used before
# this put the .exe one level too shallow, and every relative path this
# process resolves silently pointed one directory too high as a result.
xen_add_game_executable(XED
        # Win32 Resource
        Resource/App.rc
        Resource/Resource.h
        # Vendor
        ${VENDOR_ROOT}/nlohmann/json.hpp
        ${VENDOR_ROOT}/stb_image.cpp
        ${VENDOR_ROOT}/stb_image.h
        ${VENDOR_ROOT}/ini.h
        ${VENDOR_ROOT}/cgltf.cpp
        ${VENDOR_ROOT}/cgltf.h
        ${IMGUI_SOURCES}
        ${RESOURCE_HEADERS}
        # Editor
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/Editor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/Editor.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/EditorConfig.hpp
)

target_compile_definitions(XED PUBLIC
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
)

target_link_libraries(XED PRIVATE
        Xen::Common
        Xen::PAK
        Xen::Xen
        Microsoft::DirectX-Headers
        Microsoft::DirectX-Guids
        D3D12MemoryAllocator
        d3d12.lib
        dxgi.lib
        dxguid.lib
        d3dcompiler.lib
)

target_include_directories(XED PRIVATE ${MODULES_ROOT} ${VENDOR_ROOT} ${VENDOR_ROOT}/imgui)

# Reuses Sandbox's own Content/ as XED's test content for this editor slice
# (an embedded Game needs *something* to mount and render) - packed into
# XED's own Data.pxk, entirely separate from Sandbox's. The real project/
# content system (loading an arbitrary project's own content root at
# runtime) is later, separate work - see the editor plan.
xen_configure_game(XED
        PAK_FILENAME "Data.pxk"
        CONTENT_DIRS "${CMAKE_SOURCE_DIR}/Source/Sandbox/Content"
)
xen_compile_shaders(XED)
xen_package_game_content(XED CONTENT_DIR "${CMAKE_SOURCE_DIR}/Source/Sandbox/Content")