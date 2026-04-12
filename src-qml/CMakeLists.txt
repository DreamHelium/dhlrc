project(dhlrc_qml)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(ECMFindQmlModule)
include(ECMQmlModule6)

set(CMAKE_AUTOMOC TRUE)
set(CMAKE_AUTORCC TRUE)

find_package(Qt6 REQUIRED COMPONENTS
        Core
        Quick
        Gui
        QuickControls2)

find_package(KF6 REQUIRED COMPONENTS
        Kirigami
        I18n
        CoreAddons
        QQC2DesktopStyle
        IconThemes
        ConfigWidgets
)

ecm_find_qmlmodule(org.kde.kirigami REQUIRED)

add_executable(dhlrc_qml)

ecm_add_qml_module(dhlrc_qml URI cn.dh.dhlrc SOURCES dhnbt.h dhnbt.cpp
dhtreefilter.h dhtreefilter.cpp dhrealconfigdialog.h dhrealconfigdialog.cpp
)

target_sources(dhlrc_qml
        PRIVATE
        main.cpp
        region.h
        nbtvec.h
        ../resources/dhlrc.qrc
)

ecm_target_qml_sources(dhlrc_qml
        SOURCES
        Main.qml
        NbtReader.qml
        Manage.qml
)

target_link_directories(dhlrc_qml PRIVATE ${CMAKE_BINARY_DIR})
target_include_directories(dhlrc_qml PRIVATE ../src/dhconfigdialog/src ${PROJECT_BINARY_DIR})

kconfig_add_kcfg_files(dhlrc_qml ../src/config/settings.kcfgc)

target_link_libraries(dhlrc_qml
        PRIVATE
        Qt6::Quick
        Qt6::Qml
        Qt6::Gui
        Qt6::QuickControls2
        Qt6::Widgets
        KF6::I18n
        KF6::CoreAddons
        KF6::IconThemes
        region_rs
        nbt_to_vec
        dhconfigdialog
        KF6::ConfigWidgets
)
