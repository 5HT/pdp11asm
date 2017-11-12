// PDP11 Assembler (c) 08-01-2017 ALeksey Morozov (aleksey.f.morozov@gmail.com)

#include "c_asm_8080.h"

namespace C
{

Asm8080::Asm8080(Compiler& _c) : c(_c) {
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::call(const char* name, uint16_t addr)
{
    c.out.write8(0xCD);
    if(step==1 && name[0] != 0) c.addFixup(Compiler::ftWord, ucase(name)); //! Так нельзя
    c.out.write16(addr);
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::addLocalFixup(unsigned label)
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

void Asm8080::addLocalLabel(unsigned n)
{
    assure_and_fast_null(labels, n);
    labels[n] = c.out.writePtr;
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::step0()
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

void Asm8080::step1()
{
    fs = 0;
    step = 1;
    labelsCnt = 0;
    c.out.writePtr = step_pos;
    c.out.max = step_mac;
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::step2()
{
    for(unsigned i=0; i<fixups.size(); i++)
    {
        Fixup& f = fixups[i];
        if(f.label >= labels.size()) throw std::runtime_error("fixup1");
        unsigned lp = labels[f.label];
        if(lp == (unsigned)-1) throw std::runtime_error("fixup2");
        *(uint16_t*)(c.out.writeBuf + f.addr) = lp;
    }
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::addFixup(unsigned label)
{
    // Второй проход
    if(step == 1)
    {
        if(fs >= fixups.size()) throw std::runtime_error("fixup0");
        Fixup& f = fixups[fs];
        f.addr = c.out.writePtr;
        fs++;
        return;
    }

    fixups.push_back(Fixup(c.out.writePtr, label, 0));
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::mov_argN_ptr1(char pf, unsigned a)
{
    switch(pf)
    {
        case 'B':
            mov(a==0 ? Asm8080::r8_a : Asm8080::r8_e, Asm8080::r8_m);
            break;
        case 'W':
            if(a==0)
            {
                mov(Asm8080::r8_d, Asm8080::r8_m);
                inx(Asm8080::r16_hl);
                mov(Asm8080::r8_h, Asm8080::r8_m);
                mov(Asm8080::r8_l, Asm8080::r8_d);
            }
            else
            {
                mov(Asm8080::r8_e, Asm8080::r8_m);
                inx(Asm8080::r16_hl);
                mov(Asm8080::r8_d, Asm8080::r8_m);
            }
            break;
        default:
            throw;
    }
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::mov_ptr1_arg2(char pf)
{
    switch(pf)
    {
        case 'B':
            mov(Asm8080::r8_m, Asm8080::r8_e);
            break;
        case 'W':
            mov(Asm8080::r8_m, Asm8080::r8_e);
            inx(Asm8080::r16_hl);
            mov(Asm8080::r8_m, Asm8080::r8_d);
            break;
        default:
            throw;
    }
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::mov_ptr1_imm(char pf, unsigned value)
{
    switch(pf)
    {
        case 'B':
            mvi(Asm8080::r8_m, value);
            return;
        case 'W':
            mvi(Asm8080::r8_m, value);
            inx(Asm8080::r16_hl);
            mvi(Asm8080::r8_m, value >> 8);
            return;
        default:
            throw;
    }
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::mov_argN_imm(char pf, unsigned d, unsigned value)
{
    switch(pf)
    {
        case 'B':
            mvi(d ? Asm8080::r8_e : Asm8080::r8_a, value);
            break;
        case 'W':
            lxi(d ? Asm8080::r16_de : Asm8080::r16_hl, value);
            break;
        default:
            throw;
    }
}

}

