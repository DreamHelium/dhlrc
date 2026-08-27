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

if (WIN32)
  set(CARGO_LIB_PREFIX "")
  set(CARGO_LIB_SUFFIX ".dll")
elseif (APPLE)
  set(CARGO_LIB_PREFIX "lib")
  set(CARGO_LIB_SUFFIX ".dylib")
else ()
  set(CARGO_LIB_PREFIX "lib")
  set(CARGO_LIB_SUFFIX ".so")
endif ()

set(CARGO_TARGET_DIR ${CMAKE_BINARY_DIR}/cargo_build)

find_program(CARGO_EXECUTABLE cargo)
find_program(CMAKE_EXECUTABLE cmake)
add_custom_target(dhlrc_cargo_build ALL
        COMMAND ${CARGO_EXECUTABLE} build --workspace ${CARGO_RELEASE_ARG}
        --target-dir ${CARGO_TARGET_DIR}
)

set(DHLRC_BUILD_LIBRARIES
  ${CARGO_TARGET_DIR}/${CARGO_TYPE}/${CARGO_LIB_PREFIX}region_rs${CARGO_LIB_SUFFIX}
  ${CARGO_TARGET_DIR}/${CARGO_TYPE}/${CARGO_LIB_PREFIX}nbt_to_vec${CARGO_LIB_SUFFIX})
set(DHLRC_LOAD_LIBRARIES
  ${CARGO_TARGET_DIR}/${CARGO_TYPE}/${CARGO_LIB_PREFIX}nbt_component${CARGO_LIB_SUFFIX})
set(DHLRC_MODULE_LIBRARIES
  ${CARGO_TARGET_DIR}/${CARGO_TYPE}/${CARGO_LIB_PREFIX}region_nbt_rs${CARGO_LIB_SUFFIX}
  ${CARGO_TARGET_DIR}/${CARGO_TYPE}/${CARGO_LIB_PREFIX}region_litematic_rs${CARGO_LIB_SUFFIX})

add_custom_command(
  COMMAND ${CMAKE_COMMAND} -E copy ${DHLRC_BUILD_LIBRARIES} ${CMAKE_BINARY_DIR}
  TARGET dhlrc_cargo_build POST_BUILD
)
add_custom_command(
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/region_module
  TARGET dhlrc_cargo_build POST_BUILD
)
add_custom_command(
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/load_module
  TARGET dhlrc_cargo_build POST_BUILD
)
add_custom_command(
  COMMAND ${CMAKE_COMMAND} -E copy ${DHLRC_LOAD_LIBRARIES} ${CMAKE_BINARY_DIR}/load_module
  TARGET dhlrc_cargo_build POST_BUILD
)
add_custom_command(
  COMMAND ${CMAKE_COMMAND} -E copy ${DHLRC_MODULE_LIBRARIES} ${CMAKE_BINARY_DIR}/region_module
  TARGET dhlrc_cargo_build POST_BUILD
)

include(GNUInstallDirs)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
#set(CMAKE_BUILD_TYPE Debug)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

find_package(Gettext)
find_package(PkgConfig)

#pkg_search_module(LUA REQUIRED lua)

set(DH_ENABLE_DEBUG OFF)
if(DH_ENABLE_DEBUG)
  add_definitions(-DDH_DEBUG_IN_IDE)
endif()

add_subdirectory(src)

# Reserved for future?
#add_subdirectory(src-qml)

#add_subdirectory(resources)

#install(FILES "${CMAKE_CURRENT_BINARY_DIR}/locale/zh_CN/LC_MESSAGES/dhlrc.mo" DESTINATION
#        "${CMAKE_INSTALL_DATAROOTDIR}/locale/zh_CN/LC_MESSAGES/")
