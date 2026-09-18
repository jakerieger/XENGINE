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

target_include_directories(XenPBRDemo PRIVATE Runtime Runtime/Source ${MODULES_ROOT} ${VENDOR_ROOT})

# TODO: Move all of this to XenGame.cmake and execute them via function(s)

# Compile engine shaders
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_custom_target(compile_engine_shaders ALL
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/Scripts/compile_engine_shaders.py"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Compiling engine shaders..."
)
add_dependencies(XenPBRDemo compile_engine_shaders)

add_custom_command(
        TARGET XenPBRDemo
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Config" "$<TARGET_FILE_DIR:XenPBRDemo>/../Config"
        COMMENT "Copying Config directory..."
)

# Copy Config directory to build output
add_custom_command(
        TARGET XenPBRDemo
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Config" "$<TARGET_FILE_DIR:XenPBRDemo>/../Config"
        COMMENT "Copying Config directory..."
)

# Create the 'Engine' directory in the build root
add_custom_command(
        TARGET XenPBRDemo
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:XenPBRDemo>/../Engine"
        COMMENT "Create engine content directory..."
)

# Pack and output shaders pak to build directory
add_custom_command(
        TARGET XenPBRDemo
        POST_BUILD
        COMMAND "${TOOLS_BIN_DIR}/PAKTool.exe" "pack" "${CMAKE_SOURCE_DIR}/Engine/shaders" "-o" "$<TARGET_FILE_DIR:XenPBRDemo>/../Engine/XEN.Shaders.xpak"
        COMMENT "Packing engine content..."
)

# Pack and output engine environment assets pak to build directory
add_custom_command(
        TARGET XenPBRDemo
        POST_BUILD
        COMMAND "${TOOLS_BIN_DIR}/PAKTool.exe" "pack" "${CMAKE_SOURCE_DIR}/Engine/environment" "-o" "$<TARGET_FILE_DIR:XenPBRDemo>/../Engine/XEN.Environment.xpak"
        COMMENT "Packing engine content..."
)

# Pack and output game content to Data1.xpak
add_custom_command(
        TARGET XenPBRDemo
        POST_BUILD
        COMMAND "${TOOLS_BIN_DIR}/PAKTool.exe" "pack" "${CMAKE_CURRENT_SOURCE_DIR}/Content" "-o" "$<TARGET_FILE_DIR:XenPBRDemo>/../Data1.xpak"
        COMMENT "Packing game content..."
)