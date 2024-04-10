cmake_minimum_required(VERSION 3.5)

project(dhlrc_qt VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        recipesui.cpp
        recipesui.h
        recipesshowui.cpp
        recipesshowui.h
        ilchooseui.cpp
        ilchooseui.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(dhlrc_qt
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        processui.h processui.cpp
        lrcfunctionui.h lrcfunctionui.cpp
        ilreaderui.h ilreaderui.cpp
        genericui.h genericui.cpp

    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET dhlrc_qt APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(dhlrc_qt SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(dhlrc_qt
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(dhlrc_qt PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

target_link_libraries(dhlrc_qt PUBLIC dhlrc)

target_include_directories(dhlrc_qt PUBLIC
        "${PROJECT_BINARY_DIR}" "${GIO_INCLUDE_DIRS}" "${dhutil_INCLUDE_DIRS}"
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.dhlrc_qt)
endif()
set_target_properties(dhlrc_qt PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS dhlrc_qt
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(dhlrc_qt)
endif()


