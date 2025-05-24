cmake_minimum_required(VERSION 3.31)
project(dhmcdir LANGUAGES C)

find_package(PkgConfig)
pkg_search_module(GIO REQUIRED gio-2.0)

add_library(dhmcdir SHARED path.h path.c
        internal_config.c
        internal_config.h)

target_include_directories(dhmcdir PUBLIC ${GIO_INCLUDE_DIRS})

target_link_libraries(dhmcdir PUBLIC dhutil_base ${GIO_LIBRARIES} cjson)
