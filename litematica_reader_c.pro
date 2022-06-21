TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

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

LIBS += -lz -lcjson
