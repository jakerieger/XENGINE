project(XENGINE)

include(XenGame)

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
        Source/main.cpp
        Source/Editor.cpp
        Source/Editor.hpp
        Source/EditorConfig.hpp
        Source/EditorTheme.cpp
        Source/EditorTheme.hpp
        Source/EditorUI.cpp
        Source/EditorUI.hpp
        Source/EditorProject.cpp
        Source/EditorProject.hpp
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

xen_configure_game(XED
        ALLOW_NO_PAK
        CONTENT_DIRS "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
xen_compile_shaders(XED)
xen_package_engine_content(XED)