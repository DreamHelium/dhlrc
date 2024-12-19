CMAKE_MINIMUM_REQUIRED(VERSION 3.16)

find_program(COMP_EXEC glib-compile-resources)
add_custom_target(dhlrc_comp_res ALL
COMMAND ${COMP_EXEC} dhlrc.xml --target=${dhlrc_BINARY_DIR}/dhlrc_resources
WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)