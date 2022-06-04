TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
           example.c \
           libnbt/nbt.c \
           litematica_region.c \
           lrc_list.c \
           recipe_util.c

HEADERS += \
           libnbt/nbt.h \
           litematica_region.h \
           lrc_list.h \
           recipe_util.h


LIBS += -lz -lcjson
DEFINES += LOAD_RECIPES
