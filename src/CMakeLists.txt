if (NOT UTIL_FOUND)
    add_subdirectory(dhutil)
endif ()

if (NOT INTERFACE_FOUND)
    add_subdirectory(nbt_interface_cpp)
endif ()

if (${INSTALL_NOW})
    add_definitions(-DLOCALEDIR="/usr/share/locale")
    add_definitions(-DMODULEDIR="/usr/lib64/dhlrc/modules")
    add_definitions(-DRESOURCEDIR="/usr/share/dhlrc/dhlrc_resources")
    add_definitions(-DRECIPESDIR="/usr/lib64/dhlrc/recipes_modules")
    message("Install Now.")
endif ()

add_subdirectory(recipe_handler)

add_library(dhlrc SHARED
        common_info.c common_info.h
        common.c common.h
        dh_bit.c dh_bit.h
        translation.h
        translation.c
        litematica_region.cpp
        litematica_region.h
        dh_mt_table.h dh_mt_table.c
        dhlrc_list.h
        dhlrc_list.c
        recipe_util.h
        recipe_util.c
        config.c
        config.h
        process_state.c process_state.h
        csv_parser.c csv_parser.h
        json_util.c
        json_util.h
        region.cpp
        region.h
        download_file.h
        download_file.c
)

add_subdirectory(feature)

target_link_libraries(dhlrc PUBLIC z cjson
        dhutil_string dhutil_file
        dhnbt_interface_cpp
)

target_include_directories(dhlrc PUBLIC
        "${GIO_INCLUDE_DIRS}" dhutil "${minizip_INCLUDE_DIRS}"
)

target_link_directories(dhlrc PUBLIC "${GIO_LIBRARY_DIRS}")

add_executable(dhlrc.bin main.c)

target_link_libraries(dhlrc.bin PUBLIC dhlrc dhlrc_feature)

set_target_properties(dhlrc.bin PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib64")

install(TARGETS dhlrc.bin
        BUNDLE DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(TARGETS dhlrc
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

add_subdirectory(qt)
#add_subdirectory(gtk)
# add_subdirectory(cli)
add_subdirectory(conv)
add_subdirectory(mcdata)
add_subdirectory(unzip)

install(TARGETS dhlrc_conv dhlrc_mcdata dhlrc_qt dhlrc_recipe
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/dhlrc/modules)
install(FILES ${CMAKE_BINARY_DIR}/dhlrc_resources DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/dhlrc)