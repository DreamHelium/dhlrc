cmake_minimum_required(VERSION 3.15)

project(dhlrc_mcdata LANGUAGES CXX)

set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/module)

set(SOURCES mcdata.cpp)
set(HEADERS mcdata.h)

add_library(dhlrc_mcdata SHARED ${SOURCES} ${HEADERS})
target_link_libraries(dhlrc_mcdata PUBLIC dhlrc dhutil_dl)