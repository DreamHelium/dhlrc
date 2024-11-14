cmake_minimum_required(VERSION 3.5)

project(dhlrc_gtk VERSION 0.3.3 LANGUAGES C)

set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/module)

pkg_search_module(GTK REQUIRED gtk4)

add_library(dhlrc_gtk SHARED
               main_gtk.c main_gtk.h
               input_dialog.c input_dialog.h
               manage_nbt.c manage_nbt.h
               manage_window.c manage_window.h
               module.c module.h)

target_include_directories(dhlrc_gtk PUBLIC "${GTK_INCLUDE_DIRS}")
target_link_libraries(dhlrc_gtk PUBLIC "${GTK_LIBRARIES}" dhlrc)

