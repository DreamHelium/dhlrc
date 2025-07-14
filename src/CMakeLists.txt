if(NOT DEFINED ${UTIL_FOUND})
    add_subdirectory(dhutil)
endif ()

if(NOT DEFINED ${INTERFACE_FOUND})
    add_subdirectory(nbt_interface_cpp)
endif()

if(${INSTALL_NOW})
    add_definitions(-DLOCALEDIR="/usr/share/locale")
    add_definitions(-DMODULEDIR="/usr/share/dhlrc/modules")
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
        dhlrc_list.h
        dhlrc_list.c
        recipe_util.h
        recipe_util.c
        #    nbt_pos.c
        #    nbt_pos.h
        config.c
        config.h
#        uncompress.c uncompress.h
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
        mcdata_feature.c
        mcdata_feature.h
)

# target_link_libraries(dhlrc_recipe_full PUBLIC dhutil cjson)

target_link_libraries(dhlrc PUBLIC z cjson
#        ${minizip_LIBRARIES}
        dhutil_base
        dhrecipe_handler dhnbt_interface_cpp
)

#if (${minizip_VERSION} LESS 4.0.0)
#    add_definitions(-DDHLRC_MZ_VERSION_LESS_THAN_FOUR)
#endif ()

target_include_directories(dhlrc PUBLIC
        "${GIO_INCLUDE_DIRS}" dhutil "${minizip_INCLUDE_DIRS}"
)

target_link_directories(dhlrc PUBLIC "${GIO_LIBRARY_DIRS}")

add_executable(dhlrc.bin main.c)

target_link_libraries(dhlrc.bin PUBLIC dhlrc gmodule-2.0)

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

install(FILES "${CMAKE_BINARY_DIR}/module/libdhlrc_conv.so"
        "${CMAKE_BINARY_DIR}/module/libdhlrc_mcdata.so"
        "${CMAKE_BINARY_DIR}/module/libdhlrc_qt.so"
        DESTINATION
        ${CMAKE_INSTALL_DATAROOTDIR}/dhlrc/modules)
install(TARGETS dhrecipe_handler
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
)