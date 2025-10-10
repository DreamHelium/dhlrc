cmake_minimum_required(VERSION 3.15)

project(dhlrc_conv LANGUAGES CXX)

set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/module)

set(SOURCES main_conv.cpp
        region_conv.cpp)
set(HEADERS main_conv.h)

add_library(dhlrc_conv SHARED ${SOURCES} ${HEADERS})
target_link_libraries(dhlrc_conv PUBLIC dhlrc dhutil_cli)