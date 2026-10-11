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
        ${RESOURCE_HEADERS}
        # Editor
        Source/main.cpp
        Source/Editor.cpp
        Source/Editor.hpp
        Source/EditorTheme.cpp
        Source/EditorTheme.hpp
        Source/EditorUI.cpp
        Source/EditorUI.hpp
        Source/BuildProcess.cpp
        Source/BuildProcess.hpp
        Source/EditorProject.cpp
        Source/EditorProject.hpp
        Source/EditorSettings.hpp
        Source/EditorSettingsModal.cpp
        Source/EditorSettingsModal.hpp
        Source/AssetIndex.cpp
        Source/AssetIndex.hpp
        Source/ContentBrowser.cpp
        Source/ContentBrowser.hpp
        Source/IconLibrary.cpp
        Source/IconLibrary.hpp
        Source/UI.hpp
        Source/PropertyEditor.cpp
        Source/PropertyEditor.hpp
        Source/ProjectFileTemplate.hpp
        Source/ProjectSettingsModal.cpp
        Source/ProjectSettingsModal.hpp
)

# Everything else XED needs (Common, PAK, D3D12, Dear ImGui, nlohmann/json,
# stb_image's implementation) comes through Xen::Xen's usage requirements -
# XED must not compile its own copy of any vendor source Xen already contains.
target_link_libraries(XED PRIVATE Xen::Xen)

# <stb_image.h> for IconLibrary - not part of Xen's public interface.
target_include_directories(XED PRIVATE ${VENDOR_ROOT})

xen_configure_game(XED
        ALLOW_NO_PAK
        CONTENT_DIRS "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
xen_compile_shaders(XED)
xen_package_engine_content(XED)

set(OUT_DIR "$<TARGET_FILE_DIR:XED>/..")
add_custom_command(
        TARGET XED POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Templates" "${OUT_DIR}/Templates"
        COMMENT "Copying project file templates..."
        VERBATIM
)