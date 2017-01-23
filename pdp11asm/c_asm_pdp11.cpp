// PDP11 Assembler (c) 08-01-2017 ALeksey Morozov (aleksey.f.morozov@gmail.com)

#include "c_asm_pdp11.h"

namespace C
{

Arg11 Arg11::r0(atReg, 0);
Arg11 Arg11::r1(atReg, 1);
Arg11 Arg11::r2(atReg, 2);
Arg11 Arg11::r3(atReg, 3);
Arg11 Arg11::r4(atReg, 4);
Arg11 Arg11::sp(atReg, 6);
Arg11 Arg11::pc(atReg, 7);
Arg11 Arg11::null(atValue, 0, 0);

//---------------------------------------------------------------------------------------------------------------------

AsmPdp11::AsmPdp11(Compiler& _c) : c(_c) {
}

//---------------------------------------------------------------------------------------------------------------------

void AsmPdp11::arg(const Arg11& a)
{
    if(a.type<6) return;
    if(step==1 && !a.str.empty()) c.addFixup(ucase(a.str));
    c.out.write16(a.value);
}

//---------------------------------------------------------------------------------------------------------------------

void AsmPdp11::cmd(Cmd11a cmd, Arg11 a, Arg11 b)
{
    // Замена BIC, BIS... на короткие аналоги
    if(a.type>=8) a.reg = 7;
    if(b.type>=8) b.reg = 7;
    c.out.write16((cmd<<12)|((a.type&7)<<9)|(a.reg<<6)|((b.type&7)<<3)|(b.reg));
    arg(a);
    arg(b);
}

//---------------------------------------------------------------------------------------------------------------------

void AsmPdp11::cmd(Cmd11b cmd, Arg11 a)
{
    if(a.type>=8) a.reg = 7;
    c.out.write16((cmd<<6)|((a.type&7)<<3)|(a.reg));
    arg(a);
}

//---------------------------------------------------------------------------------------------------------------------

void AsmPdp11::xor_r_a(unsigned r, Arg11& a)
{
    if(a.type>=8) a.reg = 7;
    c.out.write16(074000|(r<<6)|(a.reg<<3)|(a.type&7));
    arg(a);
}

//---------------------------------------------------------------------------------------------------------------------

void AsmPdp11::push(const Arg11& a)
{
    cmd(cmdMov, a, Arg11(atRegMemDec, 6));
}

//---------------------------------------------------------------------------------------------------------------------

void AsmPdp11::pushb(const Arg11& a)
{
    cmd(cmdMovb, a, Arg11(atRegMemDec, 6));
//    cmd(cmdDec, Arg11::sp);
}

//---------------------------------------------------------------------------------------------------------------------

void AsmPdp11::pop(const Arg11& a)
{
    cmd(cmdMov, Arg11(atRegMemInc, 6), a);
}

//---------------------------------------------------------------------------------------------------------------------

void AsmPdp11::ret()
{
    c.out.write16(0207);
}

//---------------------------------------------------------------------------------------------------------------------

void AsmPdp11::call(const char* name, uint16_t addr)
{
    c.out.write16(004000|(/*PC*/7<<6)|(3<<3)|(/*PC*/7));
    if(step==1 && name[0] != 0) c.addFixup(ucase(name)); //! Так нельзя
    c.out.write16(addr);
}

//---------------------------------------------------------------------------------------------------------------------

void AsmPdp11::cmd(Cmd11c co, unsigned label)
{
    // Второй проход
    if(step==1)
    {
        if(fs >= fixups.size()) throw std::runtime_error("fixup0");
        Fixup& f = fixups[fs];
        switch(f.type)
        {
            case 0: cmd(cmdMov, Arg11::null, Arg11::pc); break; // MOV
            case 1: c.out.write16((invertCmd(co) << 6) | 2); cmd(cmdMov, Arg11::null, Arg11::pc); break; // BCC+MOV
            case 2: c.out.write16(co << 6); break; // BR
            default: throw std::runtime_error("AsmPdp11.cmdc");
        }
        f.addr = c.out.writePtr-2;
        fs++;
        return;
    }

    if(co != cmdBr) c.out.write16(co << 6);
    cmd(cmdMov, Arg11::null, Arg11::pc);
    fixups.push_back(Fixup(c.out.writePtr-2, label, co==cmdBr ? 0 : 1));
}

//---------------------------------------------------------------------------------------------------------------------

void AsmPdp11::addLocalFixup(unsigned label)
{
    if(step==1)
    {
        if(fs >= fixups.size()) throw std::runtime_error("fixup0");
        fixups[fs].addr = c.out.writePtr;
        fs++;
    }
    else
    {
        fixups.push_back(Fixup(c.out.writePtr, label, 3));
    }
}

//---------------------------------------------------------------------------------------------------------------------

void AsmPdp11::addLocalLabel(unsigned n)
{
    assure_and_fast_null(labels, n);
    labels[n] = c.out.writePtr;
}

//---------------------------------------------------------------------------------------------------------------------

void AsmPdp11::step0()
{
    fs = 0;
    step = 0;
    step_pos = c.out.writePtr;
    step_mac = c.out.max;
    labelsCnt = 0;
    labels.clear();
    fixups.clear();
}

//---------------------------------------------------------------------------------------------------------------------

void AsmPdp11::step1()
{
    for(unsigned i=0; i<fixups.size(); i++)
    {
        Fixup& f = fixups[i];
        if(f.type == 3) continue;
        if(f.label >= labels.size()) throw std::runtime_error("fixup1");
        unsigned lp = labels[f.label];
        if(lp == (unsigned)-1) throw std::runtime_error("fixup2");
        int d = lp - f.addr;
        if(f.type == 0) d += 2; // Если MOV заменяется на BR, то дельту корректировать не надо. Если BR+MOV на BR, то нужно -2
        if((d & 1) != 0) throw std::runtime_error("fixup3");
        if(d>=-256 && d<=254) f.type=2;
    }

    fs = 0;
    step = 1;
    labelsCnt = 0;
    c.out.writePtr = step_pos;
    c.out.max = step_mac;
}

//---------------------------------------------------------------------------------------------------------------------

void AsmPdp11::step2()
{
    for(unsigned i=0; i<fixups.size(); i++)
    {
        Fixup& f = fixups[i];
        if(f.label >= labels.size()) throw std::runtime_error("fixup1");
        unsigned lp = labels[f.label];
        if(lp == (unsigned)-1) throw std::runtime_error("fixup2");

        if(f.type != 2)
        {
            *(uint16_t*)(c.out.writeBuf + f.addr) = lp;
        } else {
            int d = lp - (f.addr + 2);
            if((d & 1)!=0 || d<-256 || d>=256) throw std::runtime_error("far fixup");
            *(uint16_t*)(c.out.writeBuf + f.addr) |= (d / 2) & 0xFF;
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------

Cmd11c AsmPdp11::invertCmd(Cmd11c o)
{
    switch(o)
    {
        case cmdBr:  return cmdBr;
        case cmdBne: return cmdBeq;
        case cmdBeq: return cmdBne;
        case cmdBhi:  return cmdBlos;
        case cmdBlo:  return cmdBhis;
        case cmdBhis: return cmdBlo;
        case cmdBlos: return cmdBhi;
        case cmdBgt:  return cmdBle;
        case cmdBlt:  return cmdBge;
        case cmdBge:  return cmdBlt;
        case cmdBle:  return cmdBlt;
        default: throw std::runtime_error(" AsmPdp11.invertCmd");
    }
}

}
