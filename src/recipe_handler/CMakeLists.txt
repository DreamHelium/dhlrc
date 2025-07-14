cmake_minimum_required(VERSION 3.15)

project(recipe_handler LANGUAGES C)

find_package(PkgConfig)
pkg_search_module(GLIB REQUIRED glib-2.0)

add_library(dhrecipe_handler SHARED handler.h handler.c)

target_link_libraries(dhrecipe_handler PUBLIC dhutil_base cjson glib-2.0)
target_include_directories(dhrecipe_handler PUBLIC ${GLIB_INCLUDE_DIRS})

if(NOT ${UTIL_FOUND})
    target_include_directories(dhrecipe_handler PUBLIC ../dhutil)
endif ()

add_subdirectory(recipes)