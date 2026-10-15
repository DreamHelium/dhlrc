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

# The host connects to `DhSettingsDialog::saved ()` from outside this library, so
# that class is declared export/import (`dhsettingsdialog_export.h`), the way Qt
# declares its own classes. On Windows, a class that is not declared so is
# auto-imported by MinGW, and the address the host takes for one of its functions
# is then a thunk inside the host instead of the function here; Qt finds a signal
# by comparing exactly that address, so the host's `connect ()` to it fails.
include(GenerateExportHeader)
generate_export_header(dhsettingsdialog
    BASE_NAME DHSETTINGSDIALOG
    EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/dhsettingsdialog_export.h"
)

target_link_libraries(dhsettingsdialog PUBLIC
    KF6::WidgetsAddons
    KF6::I18n
    KF6::CoreAddons
    Qt6::Widgets
    Qt6::Gui
)

target_include_directories(dhsettingsdialog PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)
