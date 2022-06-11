TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
           example.c \
           libnbt/nbt.c \
           litematica_region.c \
           dhlrc_list.c \
           recipe_util.c

HEADERS += \
           libnbt/nbt.h \
           litematica_region.h \
           dhlrc_list.h \
           recipe_util.h


LIBS += -lz -lcjson
DEFINES += LOAD_RECIPES
