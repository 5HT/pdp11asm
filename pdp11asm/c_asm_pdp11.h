// PDP11 Assembler (c) 08-01-2017 Aleksey Morozov (aleksey.f.morozov@gmail.com)

#pragma once

#include <string>
#include <stdio.h>
#include "compiler.h"
#include "tools.h"

namespace C
{

enum Arg11Type
{
    atReg=0,            // Rn
    atRegMem=1,         // (Rn)
    atRegMemInc=2,      // (Rn)+
    atRegMemMemInc=3,   // @(Rn)+
    atRegMemDec=4,      // -(Rn)
    atRegMemMemDec=5,   // @-(Rn)
    atRegValueMem=6,    // X(Rn)
    atRegValueMemMem=7, // @X(Rn)
    atValue=8+2,        // (IP)+
    atValueMem=8+3      // @(IP)+
};

class Arg11 {
public:
    Arg11Type   type;
    unsigned    reg;
    unsigned    value;
    std::string str;

    inline friend bool operator != (const Arg11& a, const Arg11& b)
    {
        return a.type!=b.type || a.reg!=b.reg || a.value!=b.value || a.str!=b.str;
    }

    inline bool regUsed()
    {
        return (type == atReg || type == atRegMem || type == atRegValueMem) && reg != 6;
    }

    Arg11(Arg11Type _type, unsigned _reg, unsigned _value, std::string _str) : type(_type), reg(_reg), value(_value), str(_str) {}
    Arg11(Arg11Type _type=atReg, unsigned _reg=0, unsigned _value=0) : type(_type), reg(_reg), value(_value) {}

    static Arg11 r0,r1,r2,r3,r4,sp;
};

enum Cmd11a
{
    cmdMov=1, cmdCmp=2, cmdBit=3, cmdBic=4, cmdBis=5, cmdAdd=6,
    cmdMovb=9, cmdCmpb=10, cmdBitb=11, cmdBicb=12, cmdBisb=13, cmdSub=14
};

enum Cmd11b
{
    cmdJmp=00001, cmdSwab=00003,
    cmdClr=00050, cmdClrb=01050, cmdCom=00051, cmdComb=01051,
    cmdInc=00052, cmdIncb=01052, cmdDec=00053, cmdDecb=01053,
    cmdNeg=00054, cmdNegb=01054, cmdAdc=00055, cmdAdcb=01055,
    cmdSbc=00056, cmdSbcb=01056, cmdTst=00057, cmdTstb=01057,
    cmdRor=00060, cmdRorb=01060, cmdRol=00061, cmdRolb=01061,
    cmdAsr=00062, cmdAsrb=01062, cmdAsl=00063, cmdAslb=01063,
    cmdSxt=00067, cmdMtps=01064, cmdMfps=01067
};

enum Cmd11c {
    cmdBr=00004, cmdBne=00010, cmdBeq=00014, cmdBge=00020,
    cmdBlt=00024, cmdBgt=00030, cmdBle=00034, cmdBpl=01000,
    cmdBmi=01004, cmdBhi=01010, cmdBvs=01020, cmdBvc=01024,
    cmdBhis=01030, cmdBcc=01030, cmdBlo=01034, cmdBcs=01034,
    cmdBlos=01014
};

class AsmPdp11 {
public:    
    class Fixup {
    public:
        size_t addr;
        unsigned label;

        Fixup(size_t _addr=0, unsigned _label=0) : addr(_addr), label(_label) {}
    };

    Compiler& c;
    std::vector<Fixup> fixups;
    std::vector<size_t> labels;

    AsmPdp11(Compiler& _c);
    void arg(const Arg11& a);
    void cmd(Cmd11a cmd, Arg11 a, Arg11 b);
    void cmd(Cmd11b cmd, Arg11 a);
    void push(Arg11& a);
    void pop(Arg11& a);
    void ret();
    void call(unsigned addr);
    void call(const char* name);
    void xor_r_a(unsigned r, Arg11& a);
    void cmd(Cmd11c cmd, unsigned addLocalLabel);
    void addLocalLabel(unsigned n);
    void resetLocalLabels();
    bool processLocalLabel(Fixup& f);
    void processLocalLabels();
};

}
