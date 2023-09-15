set( DHLRC_MAIN_POT_FILE_DEPENDS 
        src/main.c
        src/main_isoc.c
        src/nbt_litereader.c
        src/litematica_region.c
        src/lrc_extend.c
        src/recipe_util.c
        src/dhlrc_list.c
	PARENT_SCOPE
)

# configure_file(dhlrc_config.h.in dhlrc_config.h)

add_executable(dhlrc
	       main.c
	       main_isoc.c
	       main.h
	       translation.h
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
	       lrc_extend.c
	       lrc_extend.h
	       nbt_pos.c
	       nbt_pos.h
	       recipe_class/recipe_general.h
	       recipe_class/recipe_general.c
	       recipe_class/recipe_shaped.h
	       recipe_class/recipe_shaped.c
	       recipe_class/recipe_shapeless.h
	       recipe_class/recipe_shapeless.c
	       recipe_class/recipe_smelting.h
	       recipe_class/recipe_smelting.c
)

#add_definitions(-DLIBNBT_USE_LIBDEFLATE)
add_definitions(-DDH_DEBUG_IN_IDE)

target_link_libraries(dhlrc PUBLIC m z ${dhutil_LIBRARIES} cjson
	"${GLIB_LIBRARIES}" "${GIO_LIBRARIES}"
#    deflate
)

target_include_directories(dhlrc PUBLIC
	"${PROJECT_BINARY_DIR}" "${GLIB_INCLUDE_DIRS}" "${GIO_INCLUDE_DIRS}" "${dhutil_INCLUDE_DIRS}"
)

target_link_directories(dhlrc PUBLIC "${dhutil_LIBRARY_DIRS}")
