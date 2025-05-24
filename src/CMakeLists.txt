set( DHLRC_MAIN_POT_FILE_DEPENDS 
        src/main.c
        src/main_isoc.c
        src/nbt_litereader.c
        src/litematica_region.c
        src/recipe_util.c
        src/dhlrc_list.c
		src/region.cpp
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
add_subdirectory(mcdir)
add_subdirectory(nbt_interface_cpp)
add_subdirectory(recipe_handler)

# add_library(dhlrc_recipe_full SHARED
# 			json_util.c json_util.h
# 		    recipe_class_ng/recipes_general.h
# 	        recipe_class_ng/recipes_general.c
# 	        recipe_class_ng/recipes_shaped.h
# 	        recipe_class_ng/recipes_shaped.c
# 	        recipe_class_ng/recipes_shapeless.h
# 	        recipe_class_ng/recipes_shapeless.c
# 	        recipe_class_ng/recipes_smelting.h
# 	        recipe_class_ng/recipes_smelting.c
# )

add_library(dhlrc SHARED
		   common_info.c common_info.h
		   common.c common.h
		   dh_bit.c dh_bit.h
		   translation.h
		   translation.c
	       litematica_region.cpp
	       litematica_region.h
	       dhlrc_list.h
	       dhlrc_list.c
	       recipe_util.h
	       recipe_util.c
	    #    nbt_pos.c
	    #    nbt_pos.h
	       config.c
	       config.h
	       uncompress.c uncompress.h
		   process_state.c process_state.h
		   csv_parser.c csv_parser.h
	    #    nbt_util.h
	    #    nbt_util.c
		   json_util.c
		   json_util.h
		   region.cpp
		   region.h
		   download_file.h
		   download_file.c
		   create_nbt.h
		   create_nbt.c
		conv_feature.c
		conv_feature.h
)

# target_link_libraries(dhlrc_recipe_full PUBLIC dhutil cjson)

target_link_libraries(dhlrc PUBLIC z cjson 
					  ${minizip_LIBRARIES} 
					  dhutil_base
					  dhrecipe_handler dhnbt_interface_cpp dhmcdir
)
# dhlrc_recipe_full
if(${minizip_VERSION} LESS 4.0.0)
	add_definitions(-DDHLRC_MZ_VERSION_LESS_THAN_FOUR)
endif()


target_include_directories(dhlrc PUBLIC
	"${GIO_INCLUDE_DIRS}" dhutil "${minizip_INCLUDE_DIRS}"
)

target_link_directories(dhlrc PUBLIC "${GIO_LIBRARY_DIRS}")

add_executable(dhlrc.bin main.c)

target_link_libraries(dhlrc.bin PUBLIC dhlrc gmodule-2.0)

set_target_properties(dhlrc.bin PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib64")

# install(TARGETS dhlrc dhlrc_recipe_full
#     BUNDLE DESTINATION .
#     LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
# )

install(TARGETS dhlrc.bin
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_subdirectory(qt)
#add_subdirectory(gtk)
# add_subdirectory(cli)
add_subdirectory(conv)
