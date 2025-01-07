cmake_minimum_required(VERSION 3.15)

project(dhlrc_cli VERSION 0.3.3 LANGUAGES C CXX)

find_package(PkgConfig)

pkg_search_module(fmt REQUIRED fmt)

set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/module)

add_library(dhlrc_cli SHARED
            main_cli.c main_cli.h
            cli_util.c cli_util.h
            manage_nbt.cpp manage_nbt.h)

target_link_libraries(dhlrc_cli PUBLIC dhlrc fmt)