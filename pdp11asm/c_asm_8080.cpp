// PDP11 Assembler (c) 08-01-2017 ALeksey Morozov (aleksey.f.morozov@gmail.com)

#include "c_asm_8080.h"
#include <limits.h>
#include "c_tree.h"

namespace C
{

//---------------------------------------------------------------------------------------------------------------------

Asm8080::Asm8080(Compiler& _c) : c(_c), in(this), labelsCnt(0)
{
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::sta(const char* name, uint16_t addr, GlobalVar* uid)
{
    in.a.save(name, addr, uid);

    /*
    // Если в HL находится адрес переменной, то можно использовать более короткую команду
    if(uid && in.hl.value == false && in.hl.uid == uid)
    {
        mov(r8_m, r8_a);
    }
    else
    {
        in.a.save(name, addr, uid);
        // STA
        c.out.write8(0x32);
        c.addFixup(Compiler::ftWord, name);
        c.out.write16(addr);
    }

    // Теперь в A находится тоже значение, что и в переменной
    in.a.setValue(uid);
    */
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::lxi(r16 arg, uint16_t imm, const char* name, GlobalVar* uid)
{
    // Выходим, если в DE находится нужный адрес
    if(in.setAddr(arg, uid)) return;

    c.out.write8(0x01 | (arg << 4));
    c.addFixup(Compiler::ftWord, name);
    c.out.write16(imm);
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::shld(const char* name, uint16_t addr, GlobalVar* uid)
{
    in.hl.save(name, addr, uid);

    /*
    // SHLD
    c.out.write8(0x22);
    c.addFixup(Compiler::ftWord, name);
    c.out.write16(addr);

    // Теперь в HL находится то же значение, что и в переменной
    in.hl.setValue(uid);
    */
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::lda(const char* name, uint16_t addr, GlobalVar* uid)
{
    // Выходим, если в A находится нужная переменная
    if(in.a.setValue(uid)) return;

    // Если в HL находится адрес переменной, то можно использовать более короткую команду
    if(uid && in.hl.value == false && in.hl.uid == uid)
    {
        c.out.write8(0x40 | (r8_a << 3) | r8_m); // mov(r8_a, r8_m);
    }
    else
    {
        // LDA
        c.out.write8(0x3A);
        c.out.write16(addr);
        c.addFixup(Compiler::ftWord, name, 2);
    }
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::lhld(const char* name, uint16_t addr, GlobalVar* uid)
{
    // Если в HL находится нужная переменная
    if(in.hl.setValue(uid)) return;

    // LHLD
    c.out.write8(0x2A);
    c.addFixup(Compiler::ftWord, name);
    c.out.write16(addr);
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::call(const char* name, uint16_t addr)
{   
    // Сохранить все переменные
    in.flush();

    // CALL
    c.out.write8(0xCD);
    if(name[0] != 0) c.addFixup(Compiler::ftWord, ucase(name));
    c.out.write16(addr);

    // Регистры могут быть испорчены
    in.no();
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::jmp_far(const char* name, uint16_t addr)
{
    // Сохранить все переменные
    in.flush();

    // CALL
    c.out.write8(0xC3);
    if(name[0] != 0) c.addFixup(Compiler::ftWord, ucase(name));
    c.out.write16(addr);

    // Регистры могут быть испорчены
    in.no();
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::addLocalFixup(unsigned label)
{
    fixups.push_back(Fixup(c.out.writePtr, label));
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::addLocalLabel(unsigned n)
{
    // Сохранить все переменные
    in.flush();

    assure_and_fast_null(labels, n);
    labels[n] = c.out.writePtr;

    // Состояние регистров не определено
    in.no();
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::cpi(unsigned cond, uint8_t imm, const char* name)
{
    // Если нас интересует ==0 или !=0, то можно использовать более короткую команду
    if(imm == 0 && name[0]==0 && (cond == oE || cond == oNE))
    {
        ora(r8_a);
    }
    else
    {
        // CPI
        c.out.write8(0xFE);
        c.out.write8(imm);
        c.addFixup(Compiler::ftByte, name, -1);
    }
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

void Asm8080::mov_ptr1_imm(char pf, unsigned value, const char* name)
{
    switch(pf)
    {
        case 'B':
            mvi(Asm8080::r8_m, value);
            c.addFixup(Compiler::ftByte, name, 1);
            return;
        case 'W':
            mvi(Asm8080::r8_m, value);
            c.addFixup(Compiler::ftByte, name, 1);
            inx(Asm8080::r16_hl);
            mvi(Asm8080::r8_m, value >> 8);
            c.addFixup(Compiler::ftByteHigh, name, 1);
            return;
        default:
            throw;
    }
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::mov_argN_imm(char pf, unsigned d, unsigned value, GlobalVar* uid)
{
    switch(pf)
    {
        case 'B':
            mvi(d ? Asm8080::r8_e : Asm8080::r8_a, value);
            break;
        case 'W':
            lxi(d ? Asm8080::r16_de : Asm8080::r16_hl, value, "", uid);
            break;
        default:
            throw;
    }
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::jmp_cc(unsigned cond, unsigned sign, unsigned label)
{
    switch(cond)
    {
        case oE:
            jz(label);
            break;
        case oNE:
            jnz(label);
            break;
        case oL:
            if(sign) throw;
            jc(label);
            break;
        case oGE:
            if(sign) throw;
            jnc(label);
            break;
        case oG:
            if(sign) throw;
            jz(labelsCnt);
            jnc(label);
            addLocalLabel(labelsCnt++);
            break;
        case oLE:
            if(sign) throw;
            jc(label);
            jz(label);
            break;
        default:
            throw;
    }
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::alu_arg1_arg2(char pf, unsigned o)
{
    switch(pf)
    {
        case 'B':
            switch(o)
            {
                case oSAdd: case oAdd: add(Asm8080::r8_e); break;
                case oSSub: case oSub: sub(Asm8080::r8_e); break;
                case oSAnd: case oAnd: ana(Asm8080::r8_e); break;
                case oSOr:  case oOr:  ora(Asm8080::r8_e); break;
                case oSXor: case oXor: xra(Asm8080::r8_e); break;
                case oSShl: case oShl: call("__SHL8");     break;
                case oSShr: case oShr: call("__SHR8");     break;
                case oSMul: case oMul: call("__MUL8");     break;
                case oSDiv: case oDiv: call("__DIV8");     break;
                case oSMod: case oMod: call("__MOD8");     break;
                default: throw;
            }
            break;
        case 'W':
            switch(o)
            {
                case oSAdd: case oAdd: dad(Asm8080::r16_de); break;
                case oSSub: case oSub: call("__SUB16");      break;
                case oSAnd: case oAnd: call("__AND16");      break;
                case oSOr:  case oOr:  call("__OR16");       break;
                case oSXor: case oXor: call("__XOR16");      break;
                case oSShl: case oShl: call("__SHL16");      break;
                case oSShr: case oShr: call("__SHR16");      break;
                case oSMul: case oMul: call("__MUL16");      break;
                case oSDiv: case oDiv: call("__DIV16");      break;
                case oSMod: case oMod: call("__MOD16");      break;
                default: throw;
            }
            break;
        default:
            throw;
    }
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::alu_byte_arg1_imm(unsigned o, uint8_t value, const char* name)
{
    switch(o)
    {
        case oAdd: adi(value); break;
        case oSub: sui(value); break;
        case oAnd: ani(value); break;
        case oOr:  ori(value); break;
        case oXor: xri(value); break;
        case oShl: throw "SHL8CONST";
        case oShr: throw "SHR8CONST";
        default: throw;
    }
    c.addFixup(Compiler::ftByte, name, 1);
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::alu_byte_arg1_pimm(unsigned o, uint16_t imm, const char* name)
{
    lxi(Asm8080::r16_hl, imm, name);
    switch(o)
    {
        case oAdd: add(r8_m); break;
        case oSub: sub(r8_m); break;
        case oAnd: ana(r8_m); break;
        case oOr:  ora(r8_m); break;
        case oXor: xra(r8_m); break;
        default: throw;
    }
}

//---------------------------------------------------------------------------------------------------------------------

bool Asm8080::post_inc(char pf, unsigned o, unsigned s, unsigned d)
{
    bool inc = (o==moPostInc || o==moIncVoid);
    switch(pf)
    {
        case 'B':
            if(s != 1) throw;
            if(o!=moIncVoid && o!=moDecVoid) mov(d ? Asm8080::r8_e : Asm8080::r8_a, Asm8080::r8_m);
            if(inc) inr(Asm8080::r8_m); else dcr(Asm8080::r8_m);
            return false;
        case 'W':
            lxi(Asm8080::r16_de, inc ? s : -s);
            call("__PADD16");
            return true;
        default:
            throw;
    }
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::inc_pimm(char pf, int value, unsigned addr, const char* name, GlobalVar* uid)
{
    switch(pf)
    {
        case 'B':
            lda(name, addr, uid);
            for(;value > 0; value--) inr(r8_a);
            for(;value < 0; value++) dcr(r8_a);
            sta(name, addr, uid);
            break;
        case 'W':
            lhld(name, addr, uid);
            for(;value > 0; value--) inx(r16_hl);
            for(;value < 0; value++) dcx(r16_hl);
            shld(name, addr);
            break;
        default:
            throw;
    }
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::pre_inc(char pf, unsigned o, unsigned s)
{
    switch(pf)
    {
        case 'B':
            if(s != 1) throw;
            if(o==moInc) inr(Asm8080::r8_m); else dcr(Asm8080::r8_m);
            break;
        case 'W':
            lxi(Asm8080::r16_de, o==moInc ? s : -s);
            call("__SADD16");
            break;
        default:
            throw;
    }
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::mov_pimm_arg1(char pf, const char* name, uint16_t addr, GlobalVar* uid)
{
    switch(pf)
    {
        case 'B':
            sta(name, addr, uid);
            break;
        case 'W':
            shld(name, addr);
            break;
        default:
            throw;
    }
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::mov_arg1_pimm(char pf, const char* name, uint16_t addr, GlobalVar* uid)
{
    switch(pf)
    {
        case 'B':
            lda(name, addr, uid);
            break;
        case 'W':
            lhld(name, addr, uid);
            break;
        default:
            throw;
    }
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::setJumpAddresses()
{
    for(auto& f : fixups)
    {
        if(f.label >= labels.size()) throw std::runtime_error("Asm8080.setJumpAddresses 1");
        unsigned ptr = labels[f.label];
        if(ptr == UINT_MAX) throw std::runtime_error("Asm8080.setJumpAddresses 2");
        if(f.addr + sizeof(uint16_t) > sizeof(c.out.writeBuf)) throw std::runtime_error("Asm8080.setJumpAddresses 3");
        *(uint16_t*)(c.out.writeBuf + f.addr) = ptr;
    }
}

//---------------------------------------------------------------------------------------------------------------------

void Asm8080::realSave(char id, uint16_t addr, const char* name, GlobalVar* uid)
{
    switch(id)
    {
        case 'a':
            // STA
            c.out.write8(0x32);
            c.addFixup(Compiler::ftWord, name);
            c.out.write16(addr);
            // Теперь в A находится тоже значение, что и в переменной
            in.a.uid=uid;
            in.a.value=true;
            break;
        case 'H':
            // SHLD
            c.out.write8(0x22);
            c.addFixup(Compiler::ftWord, name);
            c.out.write16(addr);
            // Теперь в HL находится тоже значение, что и в переменной
            in.hl.uid=uid;
            in.hl.value=true;
            break;
        default:
            throw;
    }

    //
}

//---------------------------------------------------------------------------------------------------------------------

} // namespace C
