# Headless test for game module loading. Loads SandboxModule.dll, so it needs
# the Sandbox. Run it with: ctest -C <config> -R ModuleLoad
# BuildProcess is XED's child-process runner (used for Compile); it only needs Win32.
add_executable(ModuleLoadTest main.cpp ${TOOLS_ROOT}/XED/Source/BuildProcess.cpp)
target_include_directories(ModuleLoadTest PRIVATE ${TOOLS_ROOT}/XED/Source)
target_link_libraries(ModuleLoadTest PRIVATE Xen::Xen)
add_dependencies(ModuleLoadTest SandboxModule)

target_compile_definitions(ModuleLoadTest PRIVATE
        "XEN_TEST_MODULE_PATH=\"$<TARGET_FILE:SandboxModule>\""
        # Any real DLL without the module exports
        "XEN_TEST_NOT_A_MODULE_PATH=\"$<TARGET_FILE:Xen>\""
)

set_target_properties(ModuleLoadTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Tests"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Tests"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Tests"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin/Tests"
)

# Xen.dll next to the exe
add_custom_command(TARGET ModuleLoadTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:ModuleLoadTest> $<TARGET_FILE_DIR:ModuleLoadTest>
        COMMAND_EXPAND_LISTS
)

enable_testing()
add_test(NAME ModuleLoad COMMAND ModuleLoadTest WORKING_DIRECTORY $<TARGET_FILE_DIR:ModuleLoadTest>)
