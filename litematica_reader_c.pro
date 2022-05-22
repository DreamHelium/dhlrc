TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
           nbtProcess.c\
           libnbt/nbt.c

HEADERS += nbtProcess.h \
           libnbt/nbt.h


LIBS += -lz
