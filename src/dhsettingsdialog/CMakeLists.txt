find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

find_package(KF6 REQUIRED COMPONENTS WidgetsAddons I18n CoreAddons)
find_package(Qt6 REQUIRED COMPONENTS Widgets Gui)

set(CMAKE_AUTOMOC ON)

qt_add_library(dhsettingsdialog SHARED
    dhsettingitem.h
    dhsettingsdialog.cpp dhsettingsdialog.h
    dhsettingstemplates.cpp dhsettingstemplates.h
)

target_link_libraries(dhsettingsdialog PUBLIC
    KF6::WidgetsAddons
    KF6::I18n
    KF6::CoreAddons
    Qt6::Widgets
    Qt6::Gui
)

target_include_directories(dhsettingsdialog PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
