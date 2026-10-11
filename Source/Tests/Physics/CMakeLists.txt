# Headless test for the physics layer: PhysicsWorld on its own, and the
# RigidBody/CharacterController components in a scene. Needs no window or
# render device. Run it with: ctest -C <config> -R Physics
add_executable(PhysicsTest main.cpp)
target_link_libraries(PhysicsTest PRIVATE Xen::Xen)

set_target_properties(PhysicsTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Tests"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Tests"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Tests"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin/Tests"
)

# Xen.dll next to the exe
add_custom_command(TARGET PhysicsTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:PhysicsTest> $<TARGET_FILE_DIR:PhysicsTest>
        COMMAND_EXPAND_LISTS
)

enable_testing()
add_test(NAME Physics COMMAND PhysicsTest WORKING_DIRECTORY $<TARGET_FILE_DIR:PhysicsTest>)
