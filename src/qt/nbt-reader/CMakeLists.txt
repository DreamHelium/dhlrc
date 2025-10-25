cmake_minimum_required(VERSION 3.16)

project(dhlrc_nbtreader_qt LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/module)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Svg)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Svg)

add_definitions(-DQT_NO_SIGNALS_SLOTS_KEYWORDS)

set(PROJECT_SOURCES
        nbtreaderui.cpp nbtreaderui.h nbtreaderui.ui
)

if (${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_library(dhlrc_nbtreader_qt SHARED
            MANUAL_FINALIZATION
            ${PROJECT_SOURCES}
    )
    # Define target properties for Android with Qt 6 as:
    #    set_property(TARGET dhlrc_qt APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
    #                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
    # For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else ()
    if (ANDROID)
        add_library(dhlrc_nbtreader_qt SHARED
                ${PROJECT_SOURCES}
        )
        # Define properties for Android with Qt 5 after find_package() calls as:
        #    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else ()
        add_library(dhlrc_nbtreader_qt
                ${PROJECT_SOURCES}
        )
    endif ()
endif ()

target_link_libraries(dhlrc_nbtreader_qt PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

target_link_libraries(dhlrc_nbtreader_qt PRIVATE dhlrc dhlrc_qt)

target_include_directories(dhlrc_nbtreader_qt PUBLIC
        "${PROJECT_BINARY_DIR}" "${GIO_INCLUDE_DIRS}" "${dhutil_INCLUDE_DIRS}"
)

include(GNUInstallDirs)

if (QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(dhlrc_nbtreader_qt)
endif ()
