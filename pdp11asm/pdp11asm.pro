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
           c_asm_8080.cpp \
           c_compiler_pdp11.cpp \
           c_compiler_8080.cpp \
           c_tree.cpp \
           c_parser.cpp \
           lstwriter.cpp \
           fstools.cpp \
           parser.cpp \
    make_radio86rk_rom.cpp

HEADERS += c_asm_pdp11.h \
        c_asm_8080.h \
        c_compiler_8080.h \
        c_compiler_pdp11.h \
        compiler.h \
        c_parser.h \
        c_tree.h \
        fstools.h \
        lstwriter.h \
        output.h \
        parser.h \
        tools.h \
    make_radio86rk_rom.h
