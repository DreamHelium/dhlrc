set( DHLRC_MAIN_POT_FILE_DEPENDS 
        src/main.c
        src/nbt_litereader.c
        src/litematica_region.c
	PARENT_SCOPE
)
# Currently only this one need translate

# configure_file(dhlrc_config.h.in dhlrc_config.h)

add_subdirectory(util)

set( DHUTIL_POT_FILE_DEPENDS ${DHUTIL_POT_FILE_DEPENDS} PARENT_SCOPE )

add_executable(dhlrc
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

target_link_libraries(dhlrc PUBLIC m z cjson dhutil
#    deflate
)

target_include_directories(dhlrc PUBLIC
	"${PROJECT_BINARY_DIR}"
)

