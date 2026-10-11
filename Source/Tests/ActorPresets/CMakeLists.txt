# Headless test for the editor's actor presets (XED's ActorPresets.cpp is
# compiled into the test directly, as ModuleLoad does with BuildProcess.cpp).
# Needs no window or render device. Run it with: ctest -C <config> -R ActorPresets
add_executable(ActorPresetsTest main.cpp ${TOOLS_ROOT}/XED/Source/ActorPresets.cpp)
target_include_directories(ActorPresetsTest PRIVATE ${TOOLS_ROOT}/XED/Source)
target_link_libraries(ActorPresetsTest PRIVATE Xen::Xen)

set_target_properties(ActorPresetsTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Tests"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Tests"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Tests"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin/Tests"
)

# Xen.dll next to the exe
add_custom_command(TARGET ActorPresetsTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:ActorPresetsTest> $<TARGET_FILE_DIR:ActorPresetsTest>
        COMMAND_EXPAND_LISTS
)

enable_testing()
add_test(NAME ActorPresets COMMAND ActorPresetsTest WORKING_DIRECTORY $<TARGET_FILE_DIR:ActorPresetsTest>)
