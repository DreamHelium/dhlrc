CMAKE_MINIMUM_REQUIRED(VERSION 3.16)
PROJECT(dhlrc)

option(INSTALL_NOW OFF)
option(CARGO_RELEASE OFF)

if (CARGO_RELEASE)
    message("ON")
    set(CARGO_RELEASE_ARG "--release")
    set(CARGO_TYPE "release")
else ()
    set(CARGO_TYPE "debug")
endif ()

find_program(CARGO_EXECUTABLE cargo)
find_program(CMAKE_EXECUTABLE cmake)
add_custom_target(dhlrc_cargo_build ALL
        COMMAND ${CARGO_EXECUTABLE} build --workspace ${CARGO_RELEASE_ARG}
)

if (UNIX)
    if (NOT APPLE)
        set(DHLRC_BUILD_LIBRARIES
                ${CMAKE_CURRENT_SOURCE_DIR}/target/${CARGO_TYPE}/libregion_rs.so
                ${CMAKE_CURRENT_SOURCE_DIR}/target/${CARGO_TYPE}/libnbt_to_vec.so)
        set(DHLRC_MODULE_LIBRARIES
                ${CMAKE_CURRENT_SOURCE_DIR}/target/${CARGO_TYPE}/libregion_nbt_rs.so
                ${CMAKE_CURRENT_SOURCE_DIR}/target/${CARGO_TYPE}/libregion_litematic_rs.so
        )
    endif ()
endif ()

if (WIN32)
    set(DHLRC_BUILD_LIBRARIES
            ${CMAKE_CURRENT_SOURCE_DIR}\\target\\${CARGO_TYPE}\\libregion_rs.dll
            ${CMAKE_CURRENT_SOURCE_DIR}\\target\\${CARGO_TYPE}\\libnbt_to_vec.dll)
    set(DHLRC_MODULE_LIBRARIES
            ${CMAKE_CURRENT_SOURCE_DIR}\\target\\${CARGO_TYPE}\\libregion_nbt_rs.dll
            ${CMAKE_CURRENT_SOURCE_DIR}\\target\\${CARGO_TYPE}\\libregion_litematic_rs.dll
    )
endif ()


add_custom_command(
        COMMAND cp ${DHLRC_BUILD_LIBRARIES} ${CMAKE_BINARY_DIR}
        TARGET dhlrc_cargo_build POST_BUILD
)
add_custom_command(
        COMMAND ${CMAKE_EXECUTABLE} -E make_directory ${CMAKE_BINARY_DIR}/region_module
        TARGET dhlrc_cargo_build POST_BUILD
)
add_custom_command(
        COMMAND ${CMAKE_EXECUTABLE} -E copy ${DHLRC_MODULE_LIBRARIES} ${CMAKE_BINARY_DIR}/region_module
        TARGET dhlrc_cargo_build POST_BUILD
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
