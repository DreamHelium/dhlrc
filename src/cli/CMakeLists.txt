cmake_minimum_required(VERSION 3.5)

project(dhlrc_cli VERSION 0.3.3 LANGUAGES C)

set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/module)

add_library(dhlrc_cli SHARED
            main_cli.c main_cli.h)

target_link_libraries(dhlrc_cli PUBLIC dhlrc)