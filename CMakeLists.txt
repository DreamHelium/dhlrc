CMAKE_MINIMUM_REQUIRED(VERSION 3.16)
PROJECT( dhlrc )

set(CMAKE_BUILD_TYPE Debug)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib/shared)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)

find_package(Gettext)
find_package(PkgConfig)

pkg_search_module(GIO REQUIRED gio-2.0)
pkg_search_module(gm REQUIRED gmodule-2.0)
pkg_search_module(minizip REQUIRED minizip-ng)

set(DH_ENABLE_TRANSLATION true)

if(NOT GETTEXT_FOUND)
    set(DH_ENABLE_TRANSLATION false)
endif()

if(NOT DH_ENABLE_TRANSLATION)
    add_definitions(-DDH_DISABLE_TRANSLATION)
endif()

add_definitions(-DDH_DEBUG_IN_IDE)

add_compile_options(-Wno-error=incompatible-pointer-types)

add_subdirectory( src )
add_subdirectory( resources )
if(DH_ENABLE_TRANSLATION)
    add_subdirectory( po )
endif()

#file(COPY ${dhlrc_SOURCE_DIR}/dhlrc.svg DESTINATION ${dhlrc_BINARY_DIR})
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/locale/zh_CN/LC_MESSAGES/dhlrc.mo" DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/locale/zh_CN/LC_MESSAGES/")
