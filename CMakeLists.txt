cmake_minimum_required(VERSION 3.10)

project(dhlrc LANGUAGES C VERSION 0.1.0)

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()


configure_file(dhlrc_config.h.in dhlrc_config.h)

add_subdirectory(util)

add_executable(litematica_reader_c 
	       main.c
               libnbt/nbt.h
	       libnbt/nbt.c
	       litematica_region.c
	       litematica_region.h
	       nbt_litereader.h
	       nbt_litereader.c
	       dhlrc_list.h
	       dhlrc_list.c
	       recipe_util.h
	       recipe_util.c
	       )

#add_definitions(-DLIBNBT_USE_LIBDEFLATE)
#add_definitions(-DDH_DEBUG_IN_IDE)

target_link_libraries(litematica_reader_c PUBLIC m z cjson dhutil
#    deflate
)

target_include_directories(litematica_reader_c PUBLIC
	"${PROJECT_BINARY_DIR}"
	)
