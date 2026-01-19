cmake_minimum_required(VERSION 3.17)

project(dhlrc_qt LANGUAGES CXX)

add_definitions(-DDHLRC_VERSION="${dhlrc_qt_VERSION}")
string(TIMESTAMP CURRENT_DAY %Y%m%d)
add_definitions(-DDHLRC_COMPILE_DATE=${CURRENT_DAY})

#set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/module)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
#include(KDEInstallDirs)
#include(KDECompilerSettings)
#include(KDECMakeSettings)
find_package(KF6 REQUIRED COMPONENTS ColorScheme ConfigWidgets I18n)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Svg)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Svg)

add_definitions(-DQT_NO_SIGNALS_SLOTS_KEYWORDS)

set(ui_files mainwindow.ui
        blocklistui.ui
        blockreaderui.ui
        manageui.ui
        blockshowui.ui
        regionmodifyui.ui
        palettelistui.ui
        #        paletteaddui.ui
        loadobjectui.ui)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp mainwindow.h
        blocklistui.cpp blocklistui.h
        blockreaderui.cpp blockreaderui.h
        manageui.h manageui.cpp
        dhtableview.h dhtableview.cpp
        lrchooseui.h lrchooseui.cpp
        manage.h manage.cpp
        utility.h utility.cpp
        dhtreefilter.h dhtreefilter.cpp
        generalchooseui.h
        #        propertymodifyui.cpp
        #        propertymodifyui.h
        #        propertymodifyui.ui
        blockshowui.cpp
        blockshowui.h
        regionmodifyui.cpp
        regionmodifyui.h
        palettelistui.cpp
        palettelistui.h
        paletteaddui.cpp
        paletteaddui.h
        generalchoosedialog.cpp
        generalchoosedialog.h
        dhloadobject.cpp
        dhloadobject.h
        loadregionui.cpp
        loadregionui.h
        dhlinklikebutton.cpp
        dhlinklikebutton.h
        loadobjectui.cpp
        loadobjectui.h
        saveregionui.cpp
        saveregionui.h
        dhgameconfigui.h
        dhcheckboxgroup.cpp
        dhcheckboxgroup.h
)

set(TEMP_PROJECT_SOURCES
        main.cpp
        mainwindow.cpp mainwindow.h
        loadobjectui.cpp loadobjectui.h
        manage.cpp manage.h
        manageui.cpp manageui.h
        dhtableview.cpp dhtableview.h
        dhlinklikebutton.cpp dhlinklikebutton.h
        loadregionui.cpp loadregionui.h
        ../resources/dhlrc.qrc
        generalchoosedialog.cpp generalchoosedialog.h
        blockreaderui.cpp blockreaderui.h
        utility.cpp utility.h
        regionmodifyui.cpp regionmodifyui.h
        blocklistui.cpp blocklistui.h
        blockshowui.cpp blockshowui.h
        palettelistui.cpp palettelistui.h
)

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

target_link_libraries(dhlrc_qt PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Svg KF6::ColorScheme
        KF6::ConfigWidgets KF6::I18n KF6::I18nLocaleData region_rs)
target_link_directories(dhlrc_qt PRIVATE ${CMAKE_BINARY_DIR})
target_include_directories(dhlrc_qt PUBLIC
        "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}"
)

kconfig_add_kcfg_files(dhlrc_qt config/settings.kcfgc)

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