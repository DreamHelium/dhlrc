cmake_minimum_required(VERSION 3.5)

project(dhlrc_gtk VERSION 0.3.1 LANGUAGES C)

pkg_search_module(GTK REQUIRED gtk4)

add_executable(dhlrc_gtk 
               main_gtk.c
               input_dialog.c
               input_dialog.h)

target_include_directories(dhlrc_gtk PUBLIC "${GTK_INCLUDE_DIRS}")
target_link_libraries(dhlrc_gtk PUBLIC "${GTK_LIBRARIES}" dhlrc)

