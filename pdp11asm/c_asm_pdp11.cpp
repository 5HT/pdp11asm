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
Arg11 Arg11::null(atValue, 0, 0);

//---------------------------------------------------------------------------------------------------------------------

AsmPdp11::AsmPdp11(Compiler& _c) : c(_c) {
}

//---------------------------------------------------------------------------------------------------------------------

void AsmPdp11::arg(const Arg11& a)
{
    if(a.type<6) return;
    if(!a.str.empty()) c.addFixup(ucase(a.str));
    c.out.write16(a.value);
}

//---------------------------------------------------------------------------------------------------------------------

void AsmPdp11::cmd(Cmd11a cmd, Arg11 a, Arg11 b)
{
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

void AsmPdp11::push(Arg11& a)
{
    cmd(cmdMov, a, Arg11(atRegMemDec, 6));
}

//---------------------------------------------------------------------------------------------------------------------

void AsmPdp11::pop(Arg11& a)
{
    cmd(cmdMov, a, Arg11(atRegMemInc, 6));
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
    if(name[0] != 0) c.addFixup(ucase(name)); //! Так нельзя
    c.out.write16(addr);
}

//---------------------------------------------------------------------------------------------------------------------

void AsmPdp11::cmd(Cmd11c cmd, unsigned label)
{
    fixups.push_back(Fixup(c.out.writePtr, label));
    c.out.write16(cmd << 6);
}

//---------------------------------------------------------------------------------------------------------------------

void AsmPdp11::addLocalLabel(unsigned n)
{
    assure_and_fast_null(labels, n);
    labels[n] = c.out.writePtr;

    if(c.step2)
    {
        c.lstWriter.beforeCompileLine();
        c.lstWriter.afterCompileLine2();
        c.lstWriter.appendBuffer(i2s(n).c_str());
        c.lstWriter.appendBuffer(":\r\n");
    }
}

//---------------------------------------------------------------------------------------------------------------------

void AsmPdp11::resetLocalLabels()
{
    labels.clear();
    fixups.clear();
}

//---------------------------------------------------------------------------------------------------------------------

bool AsmPdp11::processLocalLabel(Fixup& f)
{
    if(f.label >= labels.size()) return true;
    unsigned lp = labels[f.label];
    if(lp == (unsigned)-1) return true;
    int d = lp - (f.addr + 2);
    if((d & 1)!=0 || d<-256 || d>=256) throw std::runtime_error("far fixup"); //!!!!!!!!
    *(uint16_t*)(c.out.writeBuf + f.addr) |= (d / 2) & 0xFF;
    return false;
}

//---------------------------------------------------------------------------------------------------------------------

void AsmPdp11::processLocalLabels()
{
    for(unsigned i=0; i<fixups.size(); i++)
        if(processLocalLabel(fixups[i]))
            throw std::runtime_error("fixup"); //!!!!!!!!
}

}
