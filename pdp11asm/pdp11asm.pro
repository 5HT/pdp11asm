TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
           8080.cpp \
           bitmap.cpp \
           compiler.cpp \
           pdp11.cpp \
           c_asm_pdp11.cpp \
           c_compiler_pdp11.cpp \
           c_tree.cpp \
           c_parser.cpp \
           lstwriter.cpp \
           fstools.cpp \
           parser.cpp
