cmake_minimum_required(VERSION 3.15)

project(recipe_handler LANGUAGES C)

set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/module)

find_package(PkgConfig)
pkg_search_module(GLIB REQUIRED glib-2.0)

add_library(dhlrc_recipe SHARED handler.h handler.c)

target_link_libraries(dhlrc_recipe PUBLIC dhlrc)

add_subdirectory(recipes)