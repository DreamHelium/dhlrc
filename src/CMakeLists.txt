set( DHLRC_MAIN_POT_FILE_DEPENDS 
        src/main.c
        src/main_isoc.c
        src/nbt_litereader.c
        src/litematica_region.c
        src/recipe_util.c
        src/dhlrc_list.c
		src/qt/configui.cpp
        src/qt/mainwindow.cpp
        src/qt/ilreaderui.cpp
		src/qt/ilchooseui.cpp
		src/qt/recipesshowui.cpp
		src/qt/recipesui.cpp
		src/qt/showtrackui.cpp
		src/qt/nbtselectui.cpp
		src/qt/blocklistui.cpp
		src/qt/manageui.cpp
		src/qt/manage.cpp
	PARENT_SCOPE
)

# configure_file(dhlrc_config.h.in dhlrc_config.h)
add_subdirectory(dhutil)

add_definitions(-DDH_USE_OLD_INPUT_LINE_FUNC)
add_definitions(-DDH_USE_DEPRECATED_FUNCNAMES)
set(BUILD_SHARED_LIBS ON)

add_subdirectory(minizip-ng)

add_library(dhlrc_recipe_full SHARED
		    recipe_class_ng/recipes_general.h
	        recipe_class_ng/recipes_general.c
	        recipe_class_ng/recipes_shaped.h
	        recipe_class_ng/recipes_shaped.c
	        recipe_class_ng/recipes_shapeless.h
	        recipe_class_ng/recipes_shapeless.c
	        recipe_class_ng/recipes_smelting.h
	        recipe_class_ng/recipes_smelting.c
)

add_library(dhlrc SHARED
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
		   nbt_info.c
		   nbt_info.h
		   il_info.c
		   il_info.h
		   region.c
		   region.h
		   download_file.h
		   download_file.c
		   create_nbt.h
		   create_nbt.c
		   region_info.c
		   region_info.h
)

target_link_libraries(dhlrc_recipe_full PUBLIC dhutil)

target_link_libraries(dhlrc PUBLIC z cjson minizip dhutil dhlrc_recipe_full
)

target_include_directories(dhlrc PUBLIC
	"${GIO_INCLUDE_DIRS}" dhutil
)

target_link_directories(dhlrc PUBLIC "${GIO_LIBRARY_DIRS}")

add_executable(dhlrc_cli main.c)

target_link_libraries(dhlrc_cli PUBLIC dhlrc "${nc_LIBRARIES}" gmodule-2.0)

add_subdirectory(qt)
add_subdirectory(gtk)
