TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
           example.c \
           file_util.c \
           libnbt/nbt.c \
           litematica_region.c \
           dhlrc_list.c \
           recipe_util.c

HEADERS += \
           file_util.h \
           libnbt/nbt.h \
           litematica_region.h \
           dhlrc_list.h \
           recipe_util.h


LIBS += -lz -lcjson
