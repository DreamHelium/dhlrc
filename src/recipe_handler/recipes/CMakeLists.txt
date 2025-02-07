cmake_minimum_required(VERSION 3.15)

project(recipes)

set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/recipes_module)

add_library(dhvshaped SHARED shaped.c)
add_library(dhvshapeless SHARED shapeless.c)
add_library(dhvsmelting SHARED smelting.c)

target_link_libraries(dhvshaped PUBLIC dhrecipe_handler)
target_link_libraries(dhvshapeless PUBLIC dhrecipe_handler)
target_link_libraries(dhvsmelting PUBLIC dhrecipe_handler)


