cmake_minimum_required(VERSION 3.15)

project(dhlrc_unzip LANGUAGES C)

find_package(PkgConfig)
pkg_search_module(ZIP REQUIRED libzip)
pkg_search_module(GLIB REQUIRED glib-2.0)

set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/module)

set(SOURCES unzip.c)
set(HEADERS unzip.h)

add_library(dhlrc_unzip SHARED ${SOURCES} ${HEADERS})
target_link_libraries(dhlrc_unzip PUBLIC zip ${GLIB_LIBRARIES} dhutil_string)
target_include_directories(dhlrc_unzip PUBLIC ${GLIB_INCLUDE_DIRS} ../dhutil)