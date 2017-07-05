TEMPLATE = app
CONFIG -= qt
CONFIG += c++11

LIBS += -lfuse

QMAKE_CFLAGS += -D_FILE_OFFSET_BITS=64
QMAKE_CXXFLAGS += -std=c++14 $$QMAKE_CFLAGS
QMAKE_CXXFLAGS += -pipe -Os -fno-exceptions -fno-rtti -fno-threadsafe-statics
#QMAKE_CXXFLAGS += -pipe -Os
#QMAKE_CXXFLAGS += -fno-exceptions
#QMAKE_CXXFLAGS += -fno-rtti
#QMAKE_CXXFLAGS += -fno-threadsafe-statics

INCLUDEPATH += ../pdtk

SOURCES += main.cpp
