TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt
TARGET = litematica_reader_c

SOURCES += \
           dh_string_util.c \
           main.c \
           file_util.c \
           libnbt/nbt.c \
           litematica_region.c \
           dhlrc_list.c \
           nbt_litereader.c \
           recipe_util.c

HEADERS += \
           dh_string_util.h \
           file_util.h \
           libnbt/nbt.h \
           litematica_region.h \
           dhlrc_list.h \
           nbt_litereader.h \
           recipe_util.h

INCLUDEPATH += /usr/lib64/gcc/x86_64-suse-linux/12/include \
               /usr/lib64/gcc/x86_64-suse-linux/12/include-fixed

#DEFINES += DH_DEBUG_IN_IDE

LIBS += -lz -lcjson

DISTFILES += \
    CMakeLists.txt \
    lang/en_US.json \
    lang/zh_CN.json
