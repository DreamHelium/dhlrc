cmake_minimum_required(VERSION 3.15)

project(recipes)

set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/recipes_module)

add_library(dhvshaped SHARED shaped.c)
add_library(dhvshapeless SHARED shapeless.c)
add_library(dhvsmelting SHARED smelting.c)

target_link_libraries(dhvshaped PUBLIC dhlrc_recipe)
target_link_libraries(dhvshapeless PUBLIC dhlrc_recipe)
target_link_libraries(dhvsmelting PUBLIC dhlrc_recipe)


