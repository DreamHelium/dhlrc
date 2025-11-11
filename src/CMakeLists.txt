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

add_library(dhlrc SHARED
        dh_bit.c dh_bit.h
        litematica_region.cpp litematica_region.h
        region.cpp region.h
        process_state.c process_state.h
        common_info.c common_info.h
        common.c common.h
        dhlrc_list.h dhlrc_list.c
        csv_parser.c csv_parser.h
        json_util.c json_util.h
        translation.h translation.c
        download_file.h download_file.c
        feature/dh_module.c feature/dh_module.h
        feature/conv_feature.c feature/conv_feature.h
        feature/mcdata_feature.c feature/mcdata_feature.h
        feature/recipe_feature.c feature/recipe_feature.h
        feature/unzip_feature.c feature/unzip_feature.h
        script.cpp
        script.h
        global_variant.c
        global_variant.h
        public_text.h
)

target_link_libraries(dhlrc PUBLIC dhutil_string dhnbt_interface_cpp ${gm_LIBRARIES} dhutil_type dhutil_file cjson
        ${LUA_LIBRARIES} iniparser-shared)

target_include_directories(dhlrc PUBLIC "${GIO_INCLUDE_DIRS}" dhutil ${LUA_INCLUDE_DIRS})

add_executable(dhlrc.bin main.c)

target_link_libraries(dhlrc.bin PUBLIC dhlrc)

install(TARGETS dhlrc.bin
        BUNDLE DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(TARGETS dhlrc
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

add_subdirectory(qt)
add_subdirectory(conv)

#install(TARGETS dhlrc_conv dhlrc_mcdata dhlrc_qt dhlrc_recipe
#        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/dhlrc/modules)
install(FILES ${CMAKE_BINARY_DIR}/dhlrc_resources DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/dhlrc)