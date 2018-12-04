TEMPLATE = app
CONFIG -= qt
CONFIG += c++14
CONFIG += strict_c++
CONFIG += exceptions_off
CONFIG += rtti_off

# universal arguments
QMAKE_CXXFLAGS += -fno-rtti

QMAKE_CXXFLAGS_DEBUG += -O0 -g3
QMAKE_CXXFLAGS_RELEASE += -Os


#QMAKE_CXXFLAGS_RELEASE += -fno-threadsafe-statics
QMAKE_CXXFLAGS_RELEASE += -fno-asynchronous-unwind-tables
#QMAKE_CXXFLAGS_RELEASE += -fstack-protector-all
QMAKE_CXXFLAGS_RELEASE += -fstack-protector-strong

# optimizations
QMAKE_CXXFLAGS_RELEASE += -fdata-sections
QMAKE_CXXFLAGS_RELEASE += -ffunction-sections
QMAKE_LFLAGS_RELEASE += -Wl,--gc-sections

# libraries
LIBS += -lfuse

QMAKE_CFLAGS += -D_FILE_OFFSET_BITS=64
QMAKE_CXXFLAGS += $$QMAKE_CFLAGS

SOURCES += main.cpp
