CMAKE_MINIMUM_REQUIRED(VERSION 3.16)
PROJECT(dhlrc)

option(INSTALL_NOW OFF)

find_program(CARGO_EXECUTABLE cargo)
add_custom_target(dhlrc_cargo_region ALL
        COMMAND cd ../region-rs && ${CARGO_EXECUTABLE} build
)

add_custom_target(dhlrc_cargo_nbt ALL
        COMMAND cd ../region-nbt-rs && ${CARGO_EXECUTABLE} build
)

add_custom_target(dhlrc_cargo_litematic ALL
        COMMAND cd ../region-litematic-rs && ${CARGO_EXECUTABLE} build
)

include(GNUInstallDirs)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_BUILD_TYPE Debug)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

find_package(Gettext)
find_package(PkgConfig)

#pkg_search_module(LUA REQUIRED lua)

set(DH_ENABLE_TRANSLATION true)

if (NOT GETTEXT_FOUND)
    set(DH_ENABLE_TRANSLATION false)
endif ()

if (NOT DH_ENABLE_TRANSLATION)
    add_definitions(-DDH_DISABLE_TRANSLATION)
endif ()

add_definitions(-DDH_DEBUG_IN_IDE)

add_subdirectory(src)
#add_subdirectory(resources)
if (DH_ENABLE_TRANSLATION)
    add_subdirectory(po)
endif ()

#install(FILES "${CMAKE_CURRENT_BINARY_DIR}/locale/zh_CN/LC_MESSAGES/dhlrc.mo" DESTINATION
#        "${CMAKE_INSTALL_DATAROOTDIR}/locale/zh_CN/LC_MESSAGES/")
