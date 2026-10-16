CMAKE_MINIMUM_REQUIRED(VERSION 3.16)
PROJECT(dhlrc)

# The Windows install uses `.` to keep the flat layout the programme expects;
# telling CMake to normalize it keeps that from warning on newer releases.
if (POLICY CMP0177)
  cmake_policy(SET CMP0177 NEW)
endif ()

option(CARGO_RELEASE OFF)
# Build the Rust crates from the local cache only (`cargo --offline`). Useful on a
# machine that has fetched the crates once but has no network now.
option(DH_CARGO_OFFLINE "Tell cargo to work from the local crate cache only" OFF)
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

# The core features this build wants, collected so either can be turned off. A
# build with no argument passes none, and the core then carries neither the
# desktop notifications nor the geometry.
set(DHLRC_CARGO_FEATURES "")
if (DH_DESKTOP_NOTIFICATIONS)
  list(APPEND DHLRC_CARGO_FEATURES dhlrc-core/desktop-notifications)
endif ()
if (DH_BUILD_GUI)
  # The Qt view draws blocks; the terminal frontend does not, so the renderer
  # only comes along with a build that can show it.
  list(APPEND DHLRC_CARGO_FEATURES dhlrc-core/render)
endif ()
if (DHLRC_CARGO_FEATURES)
  list(JOIN DHLRC_CARGO_FEATURES "," DHLRC_CARGO_FEATURE_LIST)
  set(DHLRC_CARGO_FEATURE_ARGS --features ${DHLRC_CARGO_FEATURE_LIST})
endif ()

if (CARGO_RELEASE)
  message("ON")
  set(CARGO_RELEASE_ARG "--release")
  set(CARGO_TYPE "release")
else ()
  set(CARGO_TYPE "debug")
endif ()

if (DH_CARGO_OFFLINE)
  set(CARGO_OFFLINE_ARG "--offline")
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
#
# The module crates link `region_rs` by name but Cargo sees no dependency edge,
# so it could link a module before `region_rs` exists (its build script only adds
# a search path). Build `region_rs` on its own first; the workspace build below
# then finds it and rebuilds nothing.
add_custom_target(dhlrc_cargo_build ALL
        COMMAND ${DHLRC_CARGO_ENV} ${CARGO_EXECUTABLE} build -p region-rs ${CARGO_RELEASE_ARG}
        ${CARGO_OFFLINE_ARG}
        --target-dir ${CARGO_TARGET_DIR}
        COMMAND ${DHLRC_CARGO_ENV} ${CARGO_EXECUTABLE} build --workspace ${CARGO_RELEASE_ARG}
        ${CARGO_OFFLINE_ARG}
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
  ${CARGO_TARGET_DIR}/${CARGO_TYPE}/${CARGO_LIB_PREFIX}region_litematic_rs${CARGO_LIB_SUFFIX}
  ${CARGO_TARGET_DIR}/${CARGO_TYPE}/${CARGO_LIB_PREFIX}region_schem_rs${CARGO_LIB_SUFFIX})
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

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
# Everything the programme loads lives next to the executable: `region_rs` and
# the core, the two module directories, and the catalogue. On Windows the whole
# tree therefore goes into the prefix root; elsewhere the binaries go under the
# usual `bin`, with the rest beside them so the layout stays together.
if (WIN32)
  set(DH_INSTALL_DIR ".")
else ()
  set(DH_INSTALL_DIR "${CMAKE_INSTALL_BINDIR}")
endif ()

if (DH_BUILD_GUI)
  install(TARGETS dhlrc_qt dhsettingsdialog
          RUNTIME DESTINATION "${DH_INSTALL_DIR}"
          LIBRARY DESTINATION "${DH_INSTALL_DIR}")
endif ()

install(FILES
        "${DHLRC_TUI_BINARY}"
        "${CARGO_TARGET_DIR}/${CARGO_TYPE}/${CARGO_LIB_PREFIX}region_rs${CARGO_LIB_SUFFIX}"
        "${CARGO_TARGET_DIR}/${CARGO_TYPE}/${CARGO_LIB_PREFIX}dhlrc_core${CARGO_LIB_SUFFIX}"
        DESTINATION "${DH_INSTALL_DIR}")

install(DIRECTORY "${CMAKE_BINARY_DIR}/load_module/"
        DESTINATION "${DH_INSTALL_DIR}/load_module")
install(DIRECTORY "${CMAKE_BINARY_DIR}/region_module/"
        DESTINATION "${DH_INSTALL_DIR}/region_module")

if (DH_TRANSLATIONS)
  install(DIRECTORY "${CMAKE_BINARY_DIR}/locale/"
          DESTINATION "${DH_INSTALL_DIR}/locale")
endif ()

# ---------------------------------------------------------------------------
# Runtime for a self-contained package
# ---------------------------------------------------------------------------
# A package only runs on another machine if it carries the MSYS2 (and, for the
# Qt frontend, Qt/KF6) runtime this project does not build. A helper assembles
# it in two parts: `runtime-base` holds what the terminal frontend and the
# modules need, `runtime-gui` what the Qt frontend needs on top of that, so a
# terminal-only install stays small. Off in an MSVC build, or without `bash`.
if (WIN32 AND MINGW AND DH_BUILD_GUI)
  find_program(DH_BASH_EXECUTABLE bash)
  option(DH_PACKAGE_RUNTIME "Bundle the Qt/KF6/MSYS2 runtime so the package runs standalone" ON)
else ()
  set(DH_BASH_EXECUTABLE "")
  set(DH_PACKAGE_RUNTIME OFF)
endif ()

if (DH_PACKAGE_RUNTIME AND NOT DH_BASH_EXECUTABLE)
  message(STATUS "bash not found: DH_PACKAGE_RUNTIME off, the package will need Qt/KF6 on the target's PATH")
  set(DH_PACKAGE_RUNTIME OFF)
endif ()

set(DH_RUNTIME_BASE "${CMAKE_BINARY_DIR}/runtime-base")
set(DH_RUNTIME_GUI "${CMAKE_BINARY_DIR}/runtime-gui")

if (DH_PACKAGE_RUNTIME)
  add_custom_target(dhlrc_runtime ALL
          COMMAND ${CMAKE_COMMAND} -E make_directory "${DH_RUNTIME_BASE}" "${DH_RUNTIME_GUI}"
          COMMAND "${DH_BASH_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/deploy-msys2-runtime.sh"
                  "${DH_RUNTIME_BASE}" "${DH_RUNTIME_GUI}" "${CMAKE_BINARY_DIR}"
          DEPENDS dhlrc_cargo_build dhlrc_qt
          COMMENT "Bundling the Qt/KF6/MSYS2 runtime for packaging"
          VERBATIM)
  # The portable archive keeps the flat layout the programme expects; the
  # installer picks the two parts apart (see below).
  install(DIRECTORY "${DH_RUNTIME_BASE}/" "${DH_RUNTIME_GUI}/"
          DESTINATION "${DH_INSTALL_DIR}"
          OPTIONAL)
endif ()

# ---------------------------------------------------------------------------
# CPack: the portable archive
# ---------------------------------------------------------------------------
# `cpack` (or the `package` target) turns the install tree into a ZIP/TGZ. The
# Windows installer is built by hand instead (see below), because CPack's NSIS
# generator cannot offer a Start Menu toggle.
set(CPACK_PACKAGE_NAME "dhlrc")
set(CPACK_PACKAGE_VENDOR "DreamHelium")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Open, inspect and modify Minecraft structure files")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/DreamHelium/dhlrc")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "dhlrc")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/COPYING")

# The project's version is its build stamp, a date (see `src/version.h`). Split
# as year.monthday it stays readable and within the 16-bit fields NSIS expects.
string(TIMESTAMP DHLRC_PACKAGE_YEAR "%Y")
string(TIMESTAMP DHLRC_PACKAGE_MONTHDAY "%m%d")
set(DH_VERSION "${DHLRC_PACKAGE_YEAR}.${DHLRC_PACKAGE_MONTHDAY}")
set(CPACK_PACKAGE_VERSION "${DH_VERSION}")

if (WIN32)
  set(CPACK_GENERATOR "ZIP")
else ()
  set(CPACK_GENERATOR "TGZ")
endif ()

include(CPack)

# ---------------------------------------------------------------------------
# NSIS installer (hand-written, `installer/dhlrc.nsi.in`)
# ---------------------------------------------------------------------------
# The script copies straight from the build and runtime directories, so it can
# keep the Qt/KF6 runtime in the graphical component and the modules in their
# own. It offers the frontends, the region modules and the Start Menu as
# choices, and registers the AppUserModelID the desktop notifications need —
# none of which CPack's NSIS generator can express.
set(DH_APP_ID "cn.dh.dhlrc")

if (WIN32 AND MINGW AND DH_BUILD_GUI AND DH_PACKAGE_RUNTIME)
  find_program(DH_MAKENSIS_EXECUTABLE makensis)
  option(DH_BUILD_INSTALLER "Build the NSIS installer (needs makensis)" ON)
else ()
  set(DH_MAKENSIS_EXECUTABLE "")
  set(DH_BUILD_INSTALLER OFF)
endif ()

if (DH_BUILD_INSTALLER AND NOT DH_MAKENSIS_EXECUTABLE)
  message(STATUS "makensis not found: no installer; install mingw-w64-ucrt-x86_64-nsis to build one")
  set(DH_BUILD_INSTALLER OFF)
endif ()

if (DH_BUILD_INSTALLER)
  file(TO_NATIVE_PATH "${CMAKE_BINARY_DIR}" DH_APPDIR_NATIVE)
  file(TO_NATIVE_PATH "${DH_RUNTIME_BASE}" DH_RUNTIME_BASE_NATIVE)
  file(TO_NATIVE_PATH "${DH_RUNTIME_GUI}" DH_RUNTIME_GUI_NATIVE)
  file(TO_NATIVE_PATH "${CMAKE_BINARY_DIR}/dhlrc-${DH_VERSION}-win64-setup.exe" DH_OUTFILE_NATIVE)
  configure_file("${CMAKE_SOURCE_DIR}/installer/dhlrc.nsi.in"
                 "${CMAKE_BINARY_DIR}/installer/dhlrc.nsi" @ONLY)

  add_custom_target(dhlrc_installer
          COMMAND "${DH_MAKENSIS_EXECUTABLE}" "${CMAKE_BINARY_DIR}/installer/dhlrc.nsi"
          DEPENDS dhlrc_runtime
          COMMENT "Building the NSIS installer"
          VERBATIM)
endif ()
