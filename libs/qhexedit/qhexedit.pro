TEMPLATE = lib

QT += core gui network
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport

CONFIG += staticlib
CONFIG += debug_and_release

HEADERS += \
    src/commands.h \
    src/qhexedit.h \
    src/qhexedit_p.h \
    src/xbytearray.h

SOURCES += \
    src/commands.cpp \
    src/qhexedit.cpp \
    src/qhexedit_p.cpp \
    src/xbytearray.cpp
