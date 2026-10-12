CMAKE_MINIMUM_REQUIRED(VERSION 3.16)
PROJECT(dhlrc)

option(INSTALL_NOW OFF)
option(CARGO_RELEASE OFF)
# The core shows its notifications on the desktop itself (via `notify-rust`).
# A build without a desktop can turn it off.
option(DH_DESKTOP_NOTIFICATIONS "Show the core's notifications as desktop notifications" ON)
# The Qt Widgets GUI (`dhlrc_qt`). Turn it off for a headless build: the shared
# libraries and the terminal frontend are still built, but Qt/KF6 are not needed.
option(DH_BUILD_GUI "Build the Qt Widgets GUI (dhlrc_qt)" ON)
# The gettext catalogue: the `po/` targets (which need `xgettext`/`msgfmt`) and
# the `locale/` directory they produce. Turn it off where those tools are not
# installed and translations are not wanted, e.g. Termux without `gettext`; the
# programme then simply runs in English.
option(DH_TRANSLATIONS "Build and install the gettext translations" ON)

if (DH_DESKTOP_NOTIFICATIONS)
  # Only defined when on, so a build without it passes no extra argument.
  set(DHLRC_CARGO_FEATURE_ARGS --features dhlrc-core/desktop-notifications)
endif ()

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
  set(CARGO_EXE_SUFFIX ".exe")
elseif (APPLE)
  set(CARGO_LIB_PREFIX "lib")
  set(CARGO_LIB_SUFFIX ".dylib")
  set(CARGO_EXE_SUFFIX "")
else ()
  set(CARGO_LIB_PREFIX "lib")
  set(CARGO_LIB_SUFFIX ".so")
  set(CARGO_EXE_SUFFIX "")
endif ()

set(CARGO_TARGET_DIR ${CMAKE_BINARY_DIR}/cargo_build)

find_program(CARGO_EXECUTABLE cargo)
find_program(CMAKE_EXECUTABLE cmake)

# Rust adds no runpath of its own, so a copied binary cannot find the shared
# libraries sitting beside it and a module opened through `dlopen` cannot find
# `libregion_rs.so`. CMake gives `dhlrc_qt` the build directory as its runpath;
# the cargo crates get the same hard-coded one here. Windows finds a DLL next to
# the executable already, so it needs none.
if (NOT WIN32)
  # Keep any flags the caller already set, and add the runpath.
  if (DEFINED ENV{RUSTFLAGS} AND NOT "$ENV{RUSTFLAGS}" STREQUAL "")
    set(DHLRC_RUSTFLAGS "$ENV{RUSTFLAGS} -C link-arg=-Wl,-rpath,${CMAKE_BINARY_DIR}")
  else ()
    set(DHLRC_RUSTFLAGS "-C link-arg=-Wl,-rpath,${CMAKE_BINARY_DIR}")
  endif ()
  set(DHLRC_CARGO_ENV ${CMAKE_COMMAND} -E env "RUSTFLAGS=${DHLRC_RUSTFLAGS}")
endif ()

# cargo needs the manifest, so the target runs from the source tree rather than
# the binary dir (which may be outside it).
add_custom_target(dhlrc_cargo_build ALL
        COMMAND ${DHLRC_CARGO_ENV} ${CARGO_EXECUTABLE} build --workspace ${CARGO_RELEASE_ARG}
        ${DHLRC_CARGO_FEATURE_ARGS}
        --target-dir ${CARGO_TARGET_DIR}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set(DHLRC_BUILD_LIBRARIES
  ${CARGO_TARGET_DIR}/${CARGO_TYPE}/${CARGO_LIB_PREFIX}region_rs${CARGO_LIB_SUFFIX}
  ${CARGO_TARGET_DIR}/${CARGO_TYPE}/${CARGO_LIB_PREFIX}dhlrc_core${CARGO_LIB_SUFFIX})
set(DHLRC_LOAD_LIBRARIES
  ${CARGO_TARGET_DIR}/${CARGO_TYPE}/${CARGO_LIB_PREFIX}nbt_component${CARGO_LIB_SUFFIX})
set(DHLRC_MODULE_LIBRARIES
  ${CARGO_TARGET_DIR}/${CARGO_TYPE}/${CARGO_LIB_PREFIX}region_nbt_rs${CARGO_LIB_SUFFIX}
  ${CARGO_TARGET_DIR}/${CARGO_TYPE}/${CARGO_LIB_PREFIX}region_litematic_rs${CARGO_LIB_SUFFIX})
# The terminal frontend, staged next to `dhlrc_qt` so it finds the modules and
# the catalogue in the same directories as the Qt build.
set(DHLRC_TUI_BINARY
  ${CARGO_TARGET_DIR}/${CARGO_TYPE}/dhlrc-tui${CARGO_EXE_SUFFIX})

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
add_custom_command(
  COMMAND ${CMAKE_COMMAND} -E copy ${DHLRC_TUI_BINARY} ${CMAKE_BINARY_DIR}
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

if (DH_BUILD_GUI)
  add_subdirectory(src)
else ()
  message(STATUS "DH_BUILD_GUI is off: building only the Rust core and the terminal frontend")
endif ()

if (DH_TRANSLATIONS)
  add_subdirectory(po)
else ()
  message(STATUS "DH_TRANSLATIONS is off: no translation catalogue is built")
endif ()

# Reserved for future?
#add_subdirectory(src-qml)

#add_subdirectory(resources)

#install(FILES "${CMAKE_CURRENT_BINARY_DIR}/locale/zh_CN/LC_MESSAGES/dhlrc.mo" DESTINATION
#        "${CMAKE_INSTALL_DATAROOTDIR}/locale/zh_CN/LC_MESSAGES/")
