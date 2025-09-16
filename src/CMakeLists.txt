if (NOT UTIL_FOUND)
    set(DHUTIL_BUILD_CLI ON)
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
        dh_bit.c dh_bit.h
        litematica_region.cpp litematica_region.h
        region.cpp region.h
)

add_library(dhlrc_ext SHARED
        common_info.c common_info.h
        common.c common.h
        dhlrc_list.h dhlrc_list.c
        recipe_util.h recipe_util.c
        config.c config.h
        process_state.c process_state.h
        csv_parser.c csv_parser.h
        json_util.c json_util.h
        translation.h translation.c
        download_file.h download_file.c
)

add_subdirectory(feature)

target_link_libraries(dhlrc PUBLIC
        dhutil_string
        dhnbt_interface_cpp)

target_include_directories(dhlrc PUBLIC "${GIO_INCLUDE_DIRS}" dhutil)

target_link_libraries(dhlrc_ext PUBLIC dhutil_file dhutil_type cjson dhlrc_feature)
target_include_directories(dhlrc_ext PUBLIC ${GIO_INCLUDE_DIRS} dhutil)

add_executable(dhlrc.bin main.c)

target_link_libraries(dhlrc.bin PUBLIC dhlrc dhlrc_ext)

install(TARGETS dhlrc.bin
        BUNDLE DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(TARGETS dhlrc dhlrc_ext dhlrc_feature
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