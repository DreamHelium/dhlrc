cmake_minimum_required(VERSION 3.17)

project(dhlrc_qt LANGUAGES CXX)

if (WIN32)
  # The executable's icon is a Windows resource script, so the RC language is
  # needed (CMake only enables it when asked).
  enable_language(RC)
endif ()

add_subdirectory(dhsettingsdialog)

find_package(Intl)

string(TIMESTAMP CURRENT_DAY %Y%m%d)
add_definitions(-DDHLRC_COMPILE_DATE=${CURRENT_DAY})

#set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/module)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
#include(KDEInstallDirs)
#include(KDECompilerSettings)
#include(KDECMakeSettings)
find_package(KF6 REQUIRED COMPONENTS ColorScheme ConfigWidgets I18n WidgetsAddons CoreAddons IconThemes)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Svg Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Svg Concurrent)

add_definitions(-DQT_NO_SIGNALS_SLOTS_KEYWORDS)

set(ui_files
        blocklistui.ui
        blockreaderui.ui
        blockshowui.ui
        regionmodifyui.ui
        palettelistui.ui
        propertymodifyui.ui
        #        paletteaddui.ui
        nbtreaderui.ui
)

set(TEMP_PROJECT_SOURCES
        main.cpp
        dhtableview.cpp dhtableview.h
        dhlinklikebutton.cpp dhlinklikebutton.h
        ../resources/dhlrc.qrc
        generalchoosedialog.cpp generalchoosedialog.h
        blockreaderui.cpp blockreaderui.h
        propertymodifyui.cpp propertymodifyui.h
        utility.cpp utility.h
        windowsappid.cpp windowsappid.h
        regionmodifyui.cpp regionmodifyui.h
        blocklistui.cpp blocklistui.h
        materiallistui.cpp materiallistui.h
        blockshowui.cpp blockshowui.h
        palettelistui.cpp palettelistui.h
        nbtreaderui.cpp
        nbtreaderui.h
        dhtreefilter.cpp dhtreefilter.h
        externalnbtreaderui.cpp
        externalnbtreaderui.h
        dhgameconfigui.h
        saveregionjob.h saveregionjob.cpp
        configobjectui.cpp
        configobjectui.h
        configobjectitems.cpp
        configobjectitems.h
        dhaboutui.cpp
        dhaboutui.h
        version.cpp
        version.h
        pluginoptionsconfig.cpp
        pluginoptionsconfig.h
        dhhelpui.cpp
        dhhelpui.h
        dhdirectoryui.cpp
        dhdirectoryui.h
        manageregionui.cpp
        manageregionui.h
        regionclass.cpp regionclass.h
        loadmodule.cpp loadmodule.h
        mainwindow.h mainwindow.cpp
        dhloadjob.cpp
        dhloadjob.h
        dhbuttondelegate.cpp
        dhbuttondelegate.h
        resourcegetter.h
        resourcegetter.cpp
        dhrenderui.h dhrenderui.cpp
        regionchangesui.h regionchangesui.cpp
        dhrenderwidget.h dhrenderwidget.cpp
        dhgpuwidget.h dhgpuwidget.cpp
        dhwidget.h dhwidget.cpp
        dhcore.h dhcore.cpp
)

if(DH_ENABLE_DEBUG)
  set(TEMP_PROJECT_SOURCES
    ${TEMP_PROJECT_SOURCES}
    dhdebugwidget.h
    dhdebugwidget.cpp)
endif()

if(DH_BUILD_RENDER_CHECK)
  set(TEMP_PROJECT_SOURCES
    ${TEMP_PROJECT_SOURCES}
    dhpreview.h
    dhpreview.cpp)
endif()

if (WIN32)
  # Windows takes the programme's icon (Explorer, taskbar, Alt+Tab) from a
  # resource; the .ico sits next to the .rc, which windres finds through this
  # include directory.
  list(APPEND TEMP_PROJECT_SOURCES ${CMAKE_SOURCE_DIR}/resources/dhlrc.rc)
  set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I${CMAKE_SOURCE_DIR}/resources")
endif ()

ki18n_wrap_ui(TEMP_PROJECT_SOURCES ${ui_files})

if (${QT_VERSION_MAJOR} GREATER_EQUAL 6)
  qt_add_executable(dhlrc_qt
            MANUAL_FINALIZATION
            ${TEMP_PROJECT_SOURCES}
    )
  # Define target properties for Android with Qt 6 as:
  #    set_property(TARGET dhlrc_qt APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
  #                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
  # For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else ()
  if (ANDROID)
    add_library(dhlrc_qt SHARED
                ${TEMP_PROJECT_SOURCES}
        )
    # Define properties for Android with Qt 5 after find_package() calls as:
    #    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
  else ()
    add_executable(dhlrc_qt
                ${TEMP_PROJECT_SOURCES}
        )
  endif ()
endif ()

target_link_libraries(dhlrc_qt PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Svg
 KF6::ColorScheme
 KF6::ConfigWidgets KF6::I18n KF6::I18nLocaleData region_rs dhlrc_core Qt6::Concurrent ${Intl_LIBRARIES}
 dhsettingsdialog KF6::WidgetsAddons KF6::CoreAddons KF6::IconThemes)

if (WIN32)
  # SetCurrentProcessExplicitAppUserModelID and the registry call behind the
  # notification sender id (see windowsappid.cpp).
  target_link_libraries(dhlrc_qt PRIVATE shell32 advapi32)
endif ()

# The GPU path of the 3D view needs Qt's OpenGL widgets, which a Qt built
# without OpenGL does not have. Where the module is missing the view keeps the
# CPU picture and the program still builds: the call sites are guarded by
# `DH_HAVE_OPENGL`.
find_package(Qt${QT_VERSION_MAJOR} QUIET OPTIONAL_COMPONENTS OpenGL OpenGLWidgets)
if (TARGET Qt${QT_VERSION_MAJOR}::OpenGLWidgets)
  target_link_libraries(dhlrc_qt PRIVATE Qt${QT_VERSION_MAJOR}::OpenGLWidgets)
  target_compile_definitions(dhlrc_qt PRIVATE DH_HAVE_OPENGL)
endif()
target_link_directories(dhlrc_qt PRIVATE ${CMAKE_BINARY_DIR})
target_include_directories(dhlrc_qt PUBLIC
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/dhlrc-core/include"
)

add_dependencies(dhlrc_qt dhlrc_cargo_build)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if (${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.dhlrc_qt)
endif ()
set_target_properties(dhlrc_qt PROPERTIES
        ${BUNDLE_ID_OPTION}
        MACOSX_BUNDLE TRUE
        WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)

if (QT_VERSION_MAJOR EQUAL 6)
  qt_finalize_executable(dhlrc_qt)
endif ()
