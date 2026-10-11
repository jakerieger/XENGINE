# Headless test for prefabs: saving, instantiating, loading through the asset
# registry, rejecting bad files, and the PrefabSpawnerComponent. Needs no
# window or render device. Run it with: ctest -C <config> -R Prefab
add_executable(PrefabTest main.cpp)
target_link_libraries(PrefabTest PRIVATE Xen::Xen)

set_target_properties(PrefabTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Tests"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Tests"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Tests"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin/Tests"
)

# Xen.dll next to the exe
add_custom_command(TARGET PrefabTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:PrefabTest> $<TARGET_FILE_DIR:PrefabTest>
        COMMAND_EXPAND_LISTS
)

enable_testing()
add_test(NAME Prefab COMMAND PrefabTest WORKING_DIRECTORY $<TARGET_FILE_DIR:PrefabTest>)
