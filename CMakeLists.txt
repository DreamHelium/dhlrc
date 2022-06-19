project(litematica_reader_c LANGUAGES C)

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(litematica_reader_c 
	       example.c
	       libnbt/nbt.h
	       libnbt/nbt.c
	       litematica_region.c
	       litematica_region.h
	       dhlrc_list.h
	       dhlrc_list.c
	       recipe_util.h
	       recipe_util.c
	       file_util.h
	       file_util.c)

       target_link_libraries(litematica_reader_c PUBLIC m z cjson)
