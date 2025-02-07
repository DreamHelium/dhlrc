cmake_minimum_required(VERSION 3.15)

project(recipe_handler LANGUAGES C)

add_library(dhrecipe_handler SHARED handler.h handler.c)

target_link_libraries(dhrecipe_handler PUBLIC dhutil cjson gmodule-2.0)

add_subdirectory(recipes)