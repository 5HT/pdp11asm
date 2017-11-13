// PDP11 Assembler (c) 08-01-2017 Aleksey Morozov (aleksey.f.morozov@gmail.com)

#pragma once

#include <string>
#include <stdio.h>
#include "compiler.h"
#include "tools.h"

namespace C
{

class Asm8080 {
public:    
    class Fixup {
    public:
        size_t addr;
        unsigned label;
        Fixup(size_t _addr=0, unsigned _label=0) : addr(_addr), label(_label) {}
    };

    Compiler& c;
    unsigned labelsCnt;
    std::vector<Fixup> fixups;
    std::vector<size_t> labels;

    enum r16psw {
        r16psw_bc, r16psw_de, r16psw_hl, r16psw_psw,
    };

    enum r16 {
        r16_bc, r16_de, r16_hl, r16_sp,
    };

    enum r8 {
        r8_b, r8_c, r8_d, r8_e, r8_h, r8_l, r8_m, r8_a,
    };

    Asm8080(Compiler& _c);
    void addLocalLabel(unsigned n);
    void addLocalFixup(unsigned label);
    void setJumpAddresses();

    void push(r16psw arg) { c.out.write8(0xC5 | (arg << 4)); }
    void pop (r16psw arg) { c.out.write8(0xC1 | (arg << 4)); }
    void dad (r16 arg) { c.out.write8(0x09 | (arg << 4)); }
    void lxi (r16 arg, uint16_t imm, const char* name = "") { c.out.write8(0x01 | (arg << 4)); c.addFixup(Compiler::ftWord, ucase(name)); c.out.write16(imm); }
    void mvi (r8 arg, uint8_t imm) { c.out.write8(0x06 | (arg << 3)); c.out.write8(imm); }
    void mov (r8 arg1, r8 arg2) { c.out.write8(0x40 | (arg1 << 3) | arg2); }
    void dcx (r16 arg) { c.out.write8(0x0B | (arg << 4)); }
    void inx (r16 arg) { c.out.write8(0x03 | (arg << 4)); }
    void ret () { c.out.write8(0xC9); }
    void dcr (r8 arg) { c.out.write8(0x05 | (arg << 3)); }
    void inr (r8 arg) { c.out.write8(0x04 | (arg << 3)); }
    void jz  (unsigned arg) { c.out.write8(0xCA); addLocalFixup(arg); c.out.write16(0); }
    void jnz (unsigned arg) { c.out.write8(0xC2); addLocalFixup(arg); c.out.write16(0); }
    void jmp (unsigned arg) { c.out.write8(0xC3); addLocalFixup(arg); c.out.write16(0); }
    void jp  (unsigned arg) { c.out.write8(0xF2); addLocalFixup(arg); c.out.write16(0); }
    void jm  (unsigned arg) { c.out.write8(0xFA); addLocalFixup(arg); c.out.write16(0); }
    void jc  (unsigned arg) { c.out.write8(0xDA); addLocalFixup(arg); c.out.write16(0); }
    void jnc (unsigned arg) { c.out.write8(0xD2); addLocalFixup(arg); c.out.write16(0); }
    void jpe (unsigned arg) { c.out.write8(0xEA); addLocalFixup(arg); c.out.write16(0); }
    void jpo (unsigned arg) { c.out.write8(0xE2); addLocalFixup(arg); c.out.write16(0); }
    void sta (const char* name, uint16_t addr=0);
    void shld(const char* name, uint16_t addr=0);
    void lda(const char* name, uint16_t addr=0);
    void lhld(const char* name, uint16_t addr=0);
    void call(const char* name, uint16_t addr=0);
    void cma () { c.out.write8(0x2F); }
    void xchg() { c.out.write8(0xEB); }
    void add (r8 arg) { c.out.write8(0x80 | arg); }
    void adc (r8 arg) { c.out.write8(0x88 | arg); }
    void ana (r8 arg) { c.out.write8(0xA0 | arg); }
    void cmp (r8 arg) { c.out.write8(0xB8 | arg); }
    void sub (r8 arg) { c.out.write8(0x90 | arg); }
    void sbb (r8 arg) { c.out.write8(0x98 | arg); }
    void ora (r8 arg) { c.out.write8(0xB0 | arg); }
    void xra (r8 arg) { c.out.write8(0xA8 | arg); }
    void adi (uint8_t imm) { c.out.write8(0xC6); c.out.write8(imm); }
    void aci (uint8_t imm) { c.out.write8(0xCE); c.out.write8(imm); }
    void ani (uint8_t imm) { c.out.write8(0xE6); c.out.write8(imm); }
    void cpi_(uint8_t imm) { c.out.write8(0xFE); c.out.write8(imm); }
    void sui (uint8_t imm) { c.out.write8(0xD6); c.out.write8(imm); }
    void sbi (uint8_t imm) { c.out.write8(0xDE); c.out.write8(imm); }
    void ori (uint8_t imm) { c.out.write8(0xF6); c.out.write8(imm); }
    void xri (uint8_t imm) { c.out.write8(0xEE); c.out.write8(imm); }
    void sphl() { c.out.write8(0xF9); }

    void inc_pimm(char pf, int value, unsigned addr, const char* name);
    bool post_inc(char pf, unsigned o, unsigned s, unsigned d);
    void pre_inc(char pf, unsigned o, unsigned s);
    void cpi(unsigned cond, uint8_t imm, const char* name);
    void mov_argN_ptr1(char pf, unsigned n);
    void mov_argN_imm(char pf, unsigned d, unsigned value);
    void mov_ptr1_imm(char pf, unsigned value, const char* name);
    void mov_pimm_arg1(char pf, const char* name, uint16_t addr);
    void mov_arg1_pimm(char pf, const char* name, uint16_t addr);
    void mov_ptr1_arg2(char pf);    
    void jmp_cc(unsigned cond, unsigned sign, unsigned label);
    void alu_arg1_arg2(char pf, unsigned o);
    void alu_byte_arg1_imm(unsigned o, uint8_t value, const char* name = "");
    void alu_byte_arg1_pimm(unsigned o, uint16_t imm, const char* name = "");
};

}
