add_executable(Bin2CC main.cpp)

set_target_properties(Bin2CC PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Bin2CC")
foreach (config ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${config} config_upper)
    set_target_properties(Bin2CC PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Bin2CC")
endforeach ()

target_link_libraries(Bin2CC PRIVATE CLI11::CLI11)
