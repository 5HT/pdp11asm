// PDP11 Assembler (c) 08-01-2017 Aleksey Morozov (aleksey.f.morozov@gmail.com)

#pragma once

#include <string>
#include <stdio.h>
#include "compiler.h"
#include "tools.h"
#include "c_tree.h"

namespace C
{

class Asm8080 {
public:    
    enum r16psw {
        r16psw_bc, r16psw_de, r16psw_hl, r16psw_psw,
    };

    enum r16 {
        r16_bc, r16_de, r16_hl, r16_sp,
    };

    enum r8 {
        r8_b, r8_c, r8_d, r8_e, r8_h, r8_l, r8_m, r8_a,
    };

    class In {
    public:
        GlobalVar* uid;
        bool value;
        bool save_flag;
        const char* save_name;
        unsigned save_addr;
        Asm8080* parent;
        char object_id;

        In(Asm8080* _parent, char _object_id) { parent=_parent; object_id=_object_id; uid = 0; value = false; save_flag = false; save_name = ""; save_addr = 0; }
        void flush() { if(!save_flag) return; save_flag = false; parent->realSave(object_id, save_addr, save_name, uid); }
        bool setValue(GlobalVar* _uid)
        {
            if(_uid && uid==_uid && value==true) return true;
            flush();
            uid=_uid;
            value=true;
            return false;
        }
        bool setAddr(GlobalVar* _uid) { if(_uid && uid==_uid && value==false) return true; flush(); uid=_uid; value=false; return false; }
        void no() { flush(); uid = 0; }
        void swap(In& b) { std::swap(uid, b.uid); std::swap(value, b.value); std::swap(save_addr, b.save_addr); std::swap(save_flag, b.save_flag); std::swap(save_name, b.save_name); }
        void save(const char* _name, uint16_t _addr, GlobalVar* _uid)
        {
            if(value && save_flag && save_name == _name && save_addr == _addr && uid == _uid) return;
            flush();
            value = true; save_flag = true; save_name = _name; save_addr = _addr; uid = _uid;
        }
    };

    class Ins {
    public:
        In a, d, de, hl;

        Ins(Asm8080* parent) : a(parent,'a'), d(parent,'d'), de(parent,'D'), hl(parent,'H') {}
        void no() { a.no(); d.no(); de.no(); hl.no(); }
        void no(r16 r) { if(r==r16_hl) hl.no(); if(r==r16_de) d.no(), de.no(); }
        void no(r16psw r) { if(r==r16psw_hl) hl.no(); if(r==r16psw_de) d.no(), de.no(); if(r==r16psw_psw) a.no(); }
        void no(r8 r) { if(r==r8_a) a.no(); if(r==r8_d || r==r8_e) d.no(), de.no(); }
        void flush() { a.flush(); d.flush(); de.flush(); hl.flush(); }
        void flush(r16 r) { if(r==r16_hl) hl.flush(); if(r==r16_de) d.flush(), de.flush(); }
        void flush(r16psw r) { if(r==r16psw_hl) hl.flush(); if(r==r16psw_de) d.flush(), de.flush(); if(r==r16psw_psw) a.flush(); }
        void flush(r8 r) { if(r==r8_a) a.flush(); if(r==r8_d || r==r8_e) d.flush(), de.flush(); }
        void swap_hl_de() { hl.flush(); hl.swap(de); }
        bool setAddr(r16 r, GlobalVar* v) { if(r==r16_hl) return hl.setAddr(v); if(r==r16_de) return de.setAddr(v); return false; }
    };

    Ins in;

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

    Asm8080(Compiler& _c);
    void addLocalLabel(unsigned n);
    void addLocalFixup(unsigned label);
    void setJumpAddresses();

    void push(r16psw arg) { in.flush(arg); c.out.write8(0xC5 | (arg << 4)); in.no(arg); }
    void pop (r16psw arg) { in.flush(arg); c.out.write8(0xC1 | (arg << 4)); in.no(arg); }
    void dad (r16 arg) { in.flush(r16_hl); c.out.write8(0x09 | (arg << 4)); in.no(r16_hl); }
    void lxi (r16 arg, uint16_t imm, const char* name = "", GlobalVar* uid=0);
    void mvi (r8 arg, uint8_t imm) { in.flush(arg); c.out.write8(0x06 | (arg << 3)); c.out.write8(imm); in.no(arg); }
    void mov (r8 arg1, r8 arg2) { in.flush(arg1); c.out.write8(0x40 | (arg1 << 3) | arg2); in.no(arg1); }
    void dcx (r16 arg) { in.flush(arg); c.out.write8(0x0B | (arg << 4)); in.no(arg); }
    void inx (r16 arg) { in.flush(arg); c.out.write8(0x03 | (arg << 4)); in.no(arg); }
    void ret () { in.flush(); c.out.write8(0xC9); in.no(); }
    void dcr (r8 arg) { in.flush(arg); c.out.write8(0x05 | (arg << 3)); in.no(arg); }
    void inr (r8 arg) { in.flush(arg); c.out.write8(0x04 | (arg << 3)); in.no(arg); }
    void jz  (unsigned arg) { in.flush(); c.out.write8(0xCA); addLocalFixup(arg); c.out.write16(0); }
    void jnz (unsigned arg) { in.flush(); c.out.write8(0xC2); addLocalFixup(arg); c.out.write16(0); }
    void jmp (unsigned arg) { in.flush(); c.out.write8(0xC3); addLocalFixup(arg); c.out.write16(0); }
    void jp  (unsigned arg) { in.flush(); c.out.write8(0xF2); addLocalFixup(arg); c.out.write16(0); }
    void jm  (unsigned arg) { in.flush(); c.out.write8(0xFA); addLocalFixup(arg); c.out.write16(0); }
    void jc  (unsigned arg) { in.flush(); c.out.write8(0xDA); addLocalFixup(arg); c.out.write16(0); }
    void jnc (unsigned arg) { in.flush(); c.out.write8(0xD2); addLocalFixup(arg); c.out.write16(0); }
    void jpe (unsigned arg) { in.flush(); c.out.write8(0xEA); addLocalFixup(arg); c.out.write16(0); }
    void jpo (unsigned arg) { in.flush(); c.out.write8(0xE2); addLocalFixup(arg); c.out.write16(0); }
    void sta (const char* name, uint16_t addr=0, GlobalVar* uid=0);
    void shld(const char* name, uint16_t addr=0, GlobalVar* uid=0);
    void lda(const char* name, uint16_t addr=0, GlobalVar* uid=0);
    void lhld(const char* name, uint16_t addr=0, GlobalVar *uid=0);
    void call(const char* name, uint16_t addr=0);
    void jmp_far(const char* name, uint16_t addr=0);
    void cma () { c.out.write8(0x2F); in.no(r8_a); }
    void xchg() { c.out.write8(0xEB); in.swap_hl_de(); }
    void cmp (r8 arg) { c.out.write8(0xB8 | arg); }
    void add (r8 arg) { in.flush(r8_a); c.out.write8(0x80 | arg); in.no(r8_a); }
    void adc (r8 arg) { in.flush(r8_a); c.out.write8(0x88 | arg); in.no(r8_a); }
    void ana (r8 arg) { in.flush(r8_a); c.out.write8(0xA0 | arg); if(arg != r8_a) in.no(r8_a); }
    void sub (r8 arg) { in.flush(r8_a); c.out.write8(0x90 | arg); in.no(r8_a); }
    void sbb (r8 arg) { in.flush(r8_a); c.out.write8(0x98 | arg); in.no(r8_a); }
    void ora (r8 arg) { in.flush(r8_a); c.out.write8(0xB0 | arg); if(arg != r8_a) in.no(r8_a); }
    void xra (r8 arg) { in.flush(r8_a); c.out.write8(0xA8 | arg); in.no(r8_a); }
    void adi (uint8_t imm) { in.flush(r8_a); c.out.write8(0xC6); c.out.write8(imm); in.no(r8_a); }
    void aci (uint8_t imm) { in.flush(r8_a); c.out.write8(0xCE); c.out.write8(imm); in.no(r8_a); }
    void ani (uint8_t imm) { in.flush(r8_a); c.out.write8(0xE6); c.out.write8(imm); in.no(r8_a); }
    void sui (uint8_t imm) { in.flush(r8_a); c.out.write8(0xD6); c.out.write8(imm); in.no(r8_a); }
    void sbi (uint8_t imm) { in.flush(r8_a); c.out.write8(0xDE); c.out.write8(imm); in.no(r8_a); }
    void ori (uint8_t imm) { in.flush(r8_a); c.out.write8(0xF6); c.out.write8(imm); in.no(r8_a); }
    void xri (uint8_t imm) { in.flush(r8_a); c.out.write8(0xEE); c.out.write8(imm); in.no(r8_a); }
    void sphl() { c.out.write8(0xF9); }

    void realSave(char id, uint16_t addr, const char* name, GlobalVar* uid);

    void inc_pimm(char pf, int value, unsigned addr, const char* name, GlobalVar *uid=0);
    bool post_inc(char pf, unsigned o, unsigned s, unsigned d);
    void pre_inc(char pf, unsigned o, unsigned s);
    void cpi(unsigned cond, uint8_t imm, const char *name);
    void mov_argN_ptr1(char pf, unsigned n);
    void mov_argN_imm(char pf, unsigned d, unsigned value, GlobalVar *uid=0);
    void mov_ptr1_imm(char pf, unsigned value, const char* name);
    void mov_pimm_arg1(char pf, const char* name, uint16_t addr, GlobalVar* uid=0);
    void mov_arg1_pimm(char pf, const char* name, uint16_t addr, GlobalVar* uid=0);
    void mov_ptr1_arg2(char pf);    
    void jmp_cc(unsigned cond, unsigned sign, unsigned label);
    void alu_arg1_arg2(char pf, unsigned o);
    void alu_byte_arg1_imm(unsigned o, uint8_t value, const char* name = "");
    void alu_byte_arg1_pimm(unsigned o, uint16_t imm, const char* name = "");
};

}
