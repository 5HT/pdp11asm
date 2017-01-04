TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += pdp11asm.cpp \
           c_8080.cpp \
           c_bitmap.cpp \
           c_common.cpp \
           c_pdp11.cpp \
           lstwriter.cpp \
           fstools.cpp \
           parser.cpp
