set( DHLRC_MAIN_POT_FILE_DEPENDS 
        src/main.c
        src/main_isoc.c
        src/nbt_litereader.c
        src/litematica_region.c
        src/lrc_extend.c
        src/recipe_util.c
        src/dhlrc_list.c
        src/recipe_class/recipe_general.c
        src/recipe_class/recipe_shaped.c
        src/qt/mainwindow.cpp
        src/qt/ilreaderui.cpp
	PARENT_SCOPE
)

# configure_file(dhlrc_config.h.in dhlrc_config.h)

add_library(dhlrc SHARED
	       main.h
		   translation.h
		   translation.c
	       libnbt/nbt.h
	       libnbt/nbt.c
	       litematica_region.c
	       litematica_region.h
	       dhlrc_list.h
	       dhlrc_list.c
	       recipe_util.h
	       recipe_util.c
	       nbt_pos.c
	       nbt_pos.h
	       config.c
	       config.h
	       uncompress.c
	       uncompress.h
	       nbt_litereader.h
	       nbt_litereader.c
		   json_util.c
		   json_util.h
	       recipe_class/recipe_general.h
	       recipe_class/recipe_general.c
	       recipe_class/recipe_shaped.h
	       recipe_class/recipe_shaped.c
	       recipe_class/recipe_shapeless.h
	       recipe_class/recipe_shapeless.c
	       recipe_class/recipe_smelting.h
	       recipe_class/recipe_smelting.c
	       recipe_class_ng/recipes_general.h
	       recipe_class_ng/recipes_general.c
	       recipe_class_ng/recipes_shaped.h
	       recipe_class_ng/recipes_shaped.c
	       recipe_class_ng/recipes_shapeless.h
	       recipe_class_ng/recipes_shapeless.c
	       recipe_class_ng/recipes_smelting.h
	       recipe_class_ng/recipes_smelting.c
)

#add_definitions(-DLIBNBT_USE_LIBDEFLATE)
#add_definitions(-DDH_DEBUG_IN_IDE)
add_definitions(-DDH_USE_OLD_INPUT_LINE_FUNC)
add_definitions(-DDH_USE_DEPRECATED_FUNCNAMES)
add_definitions(-DDH_USE_DHLRC_FILE_UTIL)
set(BUILD_SHARED_LIBS ON)

add_subdirectory(minizip-ng)

target_link_libraries(dhlrc PUBLIC m z "${dhutil_LIBRARIES}" cjson minizip
#    deflate
)

message("${dhutil_INCLUDE_DIRS}")

target_include_directories(dhlrc PUBLIC
	"${PROJECT_BINARY_DIR}" "${dhutil_INCLUDE_DIRS}"
)

target_link_directories(dhlrc PUBLIC "${dhutil_LIBRARY_DIRS}")

add_executable(dhlrc_isoc main_isoc.c main.c  lrc_extend.c lrc_extend.h)

target_link_libraries(dhlrc_isoc PUBLIC dhlrc)

install(TARGETS dhlrc
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_subdirectory(qt)
