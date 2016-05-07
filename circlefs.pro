TEMPLATE = app
#CONFIG += console c++11
#CONFIG -= app_bundle
#CONFIG -= qt
CONFIG = c++11

LIBS += -lfuse

QMAKE_CFLAGS += -D_FILE_OFFSET_BITS=64
QMAKE_CXXFLAGS += -std=c++14 $$QMAKE_CFLAGS
SOURCES += main.cpp
