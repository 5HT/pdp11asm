#include "c_compiler_8080.h"
#include "tools.h"

namespace C {

enum { B=0x10000, W=0x20000, D=0x30000 };

enum { UR_A=1, UR_HL=2 };

class UsedRegs {
public:
    unsigned old;
    Compiler8080& c;

    UsedRegs(Compiler8080* _c, char v) : c(*_c)
    {
        old = c.usedRegs;
        switch(v)
        {
            case 'B': c.usedRegs |= UR_A; break;
            case 'W': c.usedRegs |= UR_HL; break;
            default: throw;
        }
    }

    ~UsedRegs()
    {
        c.usedRegs = old;
    }
};

//---------------------------------------------------------------------------------------------------------------------

Compiler8080::Compiler8080(Compiler& _compiler, Tree& _tree) : out(_compiler), compiler(_compiler), tree(_tree)
{
    usedRegs = 0;
    curFn = 0;
}

//---------------------------------------------------------------------------------------------------------------------
// Сохранить аккумулятор

unsigned Compiler8080::pushAcc1()
{
    if(usedRegs & UR_A) { out.push(Asm8080::r16psw_psw); inStack += 2; }
    if(usedRegs & UR_HL) { out.push(Asm8080::r16psw_hl); inStack += 2; }
    return usedRegs;
}

//---------------------------------------------------------------------------------------------------------------------

void Compiler8080::popAcc(unsigned usedRegs)
{
    if(usedRegs & UR_HL) { out.pop(Asm8080::r16psw_hl); inStack -= 2; }
    if(usedRegs & UR_A) { out.pop(Asm8080::r16psw_psw); inStack -= 2; }
}

//---------------------------------------------------------------------------------------------------------------------

void Compiler8080::popAccSwap(char d, unsigned usedRegs)
{
    if(d=='B')
    {
        // Переместить результат из A в D
        out.mov(Asm8080::r8_d, Asm8080::r8_a);
    }
    else
    {
        // Переместить результат из HL в DE
        out.xchg();
    }
    if(usedRegs & UR_HL) { out.pop(Asm8080::r16psw_hl); inStack -= 2; }
    if(usedRegs & UR_A) { out.pop(Asm8080::r16psw_psw); inStack -= 2; }
}

//---------------------------------------------------------------------------------------------------------------------

void Compiler8080::compileJump(NodeVar* v, int d, bool ifTrue, unsigned label)
{
    IfOpt ifOpt;
    ifOpt.ok = false;
    ifOpt.ifTrue = ifTrue;
    ifOpt.label  = label;
    compileVar(v, d, &ifOpt);
    if(ifOpt.ok) return;

//!!!    out.cmd(cmdTst, Arg11(atReg, d));
    if(ifTrue) out.jnz(label);
         else  out.jz(label);
}

//---------------------------------------------------------------------------------------------------------------------

static inline char bwd(Type& dataType)
{
    if(dataType.is8 ()) return 'B';
    if(dataType.is16()) return 'W';
    if(dataType.is32()) return 'D';
    throw std::runtime_error("!stack");
}

//---------------------------------------------------------------------------------------------------------------------

void Compiler8080::compileVar(Node* n, unsigned d, IfOpt* ifOpt)
{
//    printf("---%u\n", n->nodeType);

    if(!n->remark.empty() && out.step==1)
        out.c.lstWriter.remark(out.c.out.writePtr, 0, n->remark.c_str());

    switch(n->nodeType)
    {
        case ntLabel: // Метка
            printf("local label %u = %u\n", n->cast<NodeLabel>()->n1, out.c.out.writePtr);
            out.addLocalLabel(n->cast<NodeLabel>()->n1);
            return;

        case ntSP: // Поместить SP в аккумулятор
            //! Проверить 16
            if(d) out.xchg();
            out.lxi(Asm8080::r16_hl, inStack);
            out.dad(Asm8080::r16_sp);
            if(d) out.xchg();
            return;

        case ntConstI: // Поместить число в аккумулятор
        {
            NodeConst* c = (NodeConst*)n;
            out.mov_argN_imm(c->dataType.b(), d, c->value);
            return;
        }

        case ntConstS: // Поместить адрес в аккумулятор
        {
            NodeConst* c = (NodeConst*)n;
            if(c->dataType.is8())
            {
                out.mvi(d ? Asm8080::r8_e : Asm8080::r8_a, 0);
                if(out.step==1) out.c.addFixup(Compiler::ftByte, n->cast<NodeConst>()->text.c_str(), 1);
            }
            else if(c->dataType.is16())
            {
                out.lxi(d ? Asm8080::r16_de : Asm8080::r16_hl, 0);
                if(out.step==1) out.c.addFixup(Compiler::ftWord, n->cast<NodeConst>()->text.c_str(), 2);
            }
            else
            {
                throw std::runtime_error("ntConstS type");
            }
            return;
        }

        case ntConvert: // Преобразование типов
        {
            NodeConvert* c = n->cast<NodeConvert>();
            compileVar(c->var, d);
            BaseType t = c->dataType.addr==0 ? c->dataType.baseType : cbtUShort;
            BaseType f = c->var->dataType.addr==0 ? c->var->dataType.baseType : cbtUShort;
            // Преобразование в меньший тип происходит незаметно.
            switch((unsigned)t)
            {
                case cbtChar:
                case cbtUChar:
                    switch((unsigned)f)
                    {
                        case cbtChar:
                        case cbtUChar:
                            break;
                        case cbtShort:
                        case cbtUShort:
                            if(d==0) out.mov(Asm8080::r8_a, Asm8080::r8_l);
                            break;
                        default: throw;
                    }
                    return;
                case cbtShort:
                case cbtUShort:
                    switch((unsigned)f)
                    {
                        case cbtShort:
                        case cbtUShort:
                            break;
                        case cbtChar:
                            out.call(d ? "__SGNB1" : "__SGNB0");
                            return;
                        case cbtUChar:
                            if(d==0) { out.mov(Asm8080::r8_l, Asm8080::r8_a); out.mvi(Asm8080::r8_h, 0); }
                                else out.mvi(Asm8080::r8_d, 0);
                            return;
                        default: throw;
                    }
                    return;
                default: throw;
                //case cbtLong:
                //case cbtULong:
                //    switch((unsigned)f)
                //    {
                //        case cbtChar:   out.call(d ? "SGNB1" : "SGNB0"); out.call(d ? "SGNW1" : "SGNW0"); return;
                //        case cbtUChar:  out.cmd(cmdBic, Arg11(atValue, 0, 0xFF00), Arg11(atReg, d)); out.cmd(cmdClr, Arg11(atReg, d+2)); return;
                //        case cbtShort:  out.call(d ? "SGNW1" : "SGNW0"); return;
                //        case cbtUShort: out.cmd(cmdClr, Arg11(atReg, d+2)); return;
                //    }
            }
            return;
        }

        case ntCall: // Вызов функции
        {
            NodeCall* c = n->cast<NodeCall>();

            char pf;
            if(c->dataType.isVoid()) pf='V'; else
            if(c->dataType.is8()) pf='B'; else
            if(c->dataType.is16()) pf='W'; else throw std::runtime_error("int");

            unsigned o;
            if(d==1) o = pushAcc1();

            unsigned ss = 0;
            unsigned regs_founded = 0;
            for(std::vector<FunctionArg>::reverse_iterator i = c->f->args.rbegin(); i != c->f->args.rend(); i++)
            {
                if(i->n >= c->args1.size()) throw std::runtime_error("ntCall args"); // Антибаг
                if(i->reg) { regs_founded++; continue; }
                NodeVar* v = c->args1[i->n];
                compileVar(v,0);
                if(v->dataType.is8()) { ss+=2; inStack+=2; out.push(Asm8080::r16psw_psw); } else
                if(v->dataType.is16()) { ss+=2; inStack+=2; out.push(Asm8080::r16psw_hl); } else throw std::runtime_error("call !stack");
            }

            if(regs_founded)
            {
                //! Константы можно не помещать в стек
                //! И вообще можно не помещать в стек переменные, которые при расчетах не использовались
                for(std::vector<FunctionArg>::reverse_iterator i = c->f->args.rbegin(); i != c->f->args.rend(); i++)
                {
                    if(!i->reg) continue;
                    regs_founded--;
                    NodeVar* v = c->args1[i->n];
                    compileVar(v,0);
                    if(!v->dataType.is8_16()) throw std::runtime_error("call !stack");
                    if(regs_founded == 0)
                    {
                        if(v->dataType.is8())
                        {
                            out.mov((Asm8080::r8)(i->reg - 1), Asm8080::r8_a);
                        }
                        else
                        {
                            if(i->reg != 9) throw "only hl";
                        }
                    }
                    else
                    {
                        ss += 2;
                        inStack += 2;
                        out.push(v->dataType.is8() ? Asm8080::r16psw_psw : Asm8080::r16psw_hl);
                    }
                }

                for(std::vector<FunctionArg>::iterator i = c->f->args.begin(); i != c->f->args.end(); i++, n++)
                {
                    if(i->reg)
                    {
                        NodeVar* v = c->args1[i->n];
                        if(regs_founded != 0)
                        {
                            if(!v->dataType.is8() || i->reg != 9) throw "only hl"; out.pop(Asm8080::r16psw_hl); ss-=2; inStack-=2;
                        }
                        regs_founded++;
                    }
                }
            }

            if(c->f && c->f->call_type == 2)
            {
                out.call("", c->f->call_arg);
            }
            else
            {
                out.call(c->name.c_str(), c->addr);
            }

            if(d == 1) popAccSwap(pf, o);

            return;
        }

        case ntDeaddr: // Получить значение по адресу
        {
            NodeDeaddr* c = (NodeDeaddr*)n;
            unsigned ur;
            if(d==1) ur = pushAcc1();
            compileVar(c->var, 0, 0); // в HL
            out.mov_argN_ptr1(c->dataType.b(), d); // e,de = [hl]
            if(d==1) popAcc(ur);
            return;
         }

        case ntSwitch: // Оптимизированый выбор
        {
            NodeSwitch* s = n->cast<NodeSwitch>();
            if(s->var) compileVar(s->var, 0);
            out.call("__switch");
            for(std::map<unsigned int, NodeLabel*>::iterator i=s->cases.begin(); i!=s->cases.end(); i++)
            {
                out.addLocalFixup(i->second->n1);
                out.c.out.write16(0);
                out.c.out.write16(i->first);
            }
            out.c.out.write16(0);
            if(s->defaultLabel == 0) throw std::runtime_error("NodeSwitch->defaultLabel");
            out.addLocalFixup(s->defaultLabel->n1);
            out.c.out.write16(0);
            compileBlock(s->body);
            return;
        }

        case ntReturn:
        {
            if(d != 0) throw std::runtime_error("return !d");
            NodeReturn* r = n->cast<NodeReturn>();
            if(r->var) compileVar(r->var, 0);
            if(curFn->stackSize != 0) out.jmp(returnLabel); else out.ret();
            return;
        }

        case ntMonoOperator:
        {
            NodeMonoOperator* o = n->cast<NodeMonoOperator>();

            if(o->o==moInc || o->o==moDec)
            {
                // Мы увеличиваем значение по адресу
                Type t = o->dataType;
                if(t.addr==0) throw std::runtime_error("monoOperator inc/dec addr==0");
                t.addr--;

                // Разрядность значения по адресу
                unsigned pp;
                if(t.is8()) pp=B; else
                if(t.is16()) pp=W; else throw std::runtime_error("monoOperator inc/dec !stack");
                unsigned s = t.sizeForInc();

                unsigned ur;
                if(d == 1) ur = pushAcc1();

                // HL - содержит адрес
                compileVar(o->a, 0);

                switch(o->o | pp)
                {
                    case moInc | B: if(s==1) { out.inr(Asm8080::r8_m); break; } throw;
                    case moDec | B: if(s==1) { out.dcr(Asm8080::r8_m); break; } throw;
                    case moInc | W: out.lxi(Asm8080::r16_de,  s); out.call("__ADDPW"); break;
                    case moDec | W: out.lxi(Asm8080::r16_de, -s); out.call("__ADDPW"); break;
                    default: throw;
                }

                if(d == 1) popAccSwap(pp, ur);
                return;
            }

            char pf;
            unsigned pp;
            if(o->a->dataType.is8()) pf='B', pp=B; else
            if(o->a->dataType.is16()) pf='W', pp=W; else throw std::runtime_error("monoOperator !stack");
            unsigned s = o->a->dataType.sizeForInc();

            if(o->o==moIncVoid || o->o==moDecVoid) // На входе адрес
            {
                assert(d == 0);
                compileVar(o->a, 0);
                switch(o->o | pp)
                {
                    case moIncVoid | B: if(s==1) { out.inr(Asm8080::r8_m); break; } throw;
                    case moDecVoid | B: if(s==1) { out.dcr(Asm8080::r8_m); break; } throw;
                    default: throw;
                }
                return;
            }

            if(o->o==moPostInc || o->o==moPostDec) // На входе адрес, на выходе значение
            {
                // Ссылка в значениях
                if(o->a->nodeType != ntDeaddr) throw std::runtime_error("monoOperator !deaddr (%u)" + i2s(o->a->nodeType));

                compileVar(o->a->cast<NodeDeaddr>()->var, 0);

                //Arg11 al, ah;
                //compileArg(false, al, ah, o->a, d, d, pf);
                //bool au = al.regUsed();

                if(pf=='B')
                {
                    out.mov(d ? Asm8080::r8_e : Asm8080::r8_a, Asm8080::r8_m);
                    if(s==1) { if(o->o==moPostInc) out.inr(Asm8080::r8_m); else out.dcr(Asm8080::r8_m); }
                        else throw;
                    return;
                }

                if(pf=='W')
                {
                    throw;
                    /*
                    out.mov(d ? Asm8080::r8_e : Asm8080::r8_a, Asm8080::r8_m);
                    out.inx(Asm8080::r16_hl);
                    out.mov(d ? Asm8080::r8_d : Asm8080::r8_d, Asm8080::r8_m);

                    out.cmd(cmdMov, al, au ? Arg11::r4 : Arg11(atReg, d));
                    if(s==1) out.cmd(o->o==moPostInc ? cmdInc : cmdDec, al);
                        else out.cmd(o->o==moPostInc ? cmdAdd : cmdSub, Arg11(atValue, 0, s), al);
                    */
                    return;
                }

                throw std::runtime_error("int");
            }

            if(o->o==moNot && ifOpt)
            {
                compileJump(o->a, d, !ifOpt->ifTrue, ifOpt->label);
                ifOpt->ok = true;
                return;
            }

            if(o->o==moNot || o->o==moNeg || o->o==moXor)
            {
                unsigned ur;
                if(d) ur = pushAcc1();
                compileVar(o->a, 0);
                switch(o->o | pp)
                {
                    case moNot | B: out.call("__NOTB"); return;
                    case moNeg | B: out.call("__NEGB"); return;
                    case moXor | B: out.cma(); return;
                    case moNot | W: out.call("__NOTW"); return;
                    case moNeg | W: out.call("__NEGW"); return;
                    case moXor | W: out.call("__XORW"); return;
                }
                out.mov(Asm8080::r8_e, Asm8080::r8_a);
                if(d) popAccSwap(pf, ur);
            }

            throw std::runtime_error("int");
        }

        case ntOperator: {
            NodeOperator* r = (NodeOperator*)n;

            Type& t = (r->o==oLAnd || r->o==oLOr || r->o==oSetVoid || r->o==oSet) ? r->dataType : r->b->dataType;
            char pf;
            unsigned pp;
            if(t.is8()) pf='B', pp=B; else
            if(t.is16()) pf='W', pp=W; else
            if(t.is32()) pf='D', pp=D; else throw std::runtime_error("int " + i2s(t.baseType));

            if(r->o==oLAnd || r->o==oLOr)
            {
                if(ifOpt)
                {
                    ifOpt->ok = true;
                    unsigned nextLabel = out.labelsCnt++;
                    if(r->o==oLAnd)
                    {
                        if(ifOpt->ifTrue)
                        {
                            compileJump(r->a, d, false, nextLabel);
                            compileJump(r->b, d, true, ifOpt->label);
                        }
                        else
                        {
                            compileJump(r->a, d, false, ifOpt->label);
                            compileJump(r->b, d, false, ifOpt->label);
                        }
                    }
                    else // oLOr
                    {
                        if(ifOpt->ifTrue)
                        {
                            compileJump(r->a, d, true, ifOpt->label);
                            compileJump(r->b, d, true, ifOpt->label);
                        }
                        else
                        {
                            compileJump(r->a, d, true, nextLabel);
                            compileJump(r->b, d, false, ifOpt->label);
                        }
                    }
                    out.addLocalLabel(nextLabel);
                    return;
                }

                unsigned trueLabel = out.labelsCnt++;
                unsigned falseLabel = out.labelsCnt++;
                unsigned nextLabel = out.labelsCnt++;

                if(r->o==oLAnd)
                {
                    compileJump(r->a, d, false, falseLabel);
                    compileJump(r->b, d, false, falseLabel);
                }
                else
                {
                    compileJump(r->a, d, true,  trueLabel);
                    compileJump(r->b, d, false, falseLabel);
                }

                out.addLocalLabel(trueLabel);
                out.mvi(d ? Asm8080::r8_e : Asm8080::r8_d, 1);
                out.jmp(nextLabel);

                out.addLocalLabel(falseLabel);
                out.mvi(d ? Asm8080::r8_e : Asm8080::r8_d, 0);

                out.addLocalLabel(nextLabel);
                return;
            }

            if(r->o==oSetVoid || r->o==oSet)
            {
                if(r->a->nodeType != ntDeaddr) throw;
                NodeDeaddr* aa = r->a->cast<NodeDeaddr>();
                compileVar(aa->var, 0, 0);
                if(r->b->nodeType == ntConstI)
                {
                    out.mov_ptr1_imm(t.b(), r->b->cast<NodeConst>()->value);
                    return;
                }
                UsedRegs ac(this, aa->var->dataType.b());
                compileVar(r->b, 1, 0);
                out.mov_ptr1_arg2(t.b());
                return;
            }

            if(r->o==oSShr || r->o==oSShl || r->o==oSAdd || r->o==oSSub || r->o==oSAnd || r->o==oSOr || r->o==oSXor)
            {
                // Ссылка в значениях
                if(r->a->nodeType != ntDeaddr) throw std::runtime_error("monoOperator !deaddr (%u)" + i2s(r->a->nodeType));

                // Адрес первой переменной
                NodeVar* d = r->a->cast<NodeDeaddr>()->var;
                compileVar(d, 0, 0);

                // Получене значения
                if(t.is8())
                {
                    out.mov(Asm8080::r8_a, Asm8080::r8_m);
                }
                else if(t.is16())
                {
                    out.mov(Asm8080::r8_e, Asm8080::r8_m);
                    out.inx(Asm8080::r16_hl);
                    out.mov(Asm8080::r8_d, Asm8080::r8_m);
                    out.xchg();
                }

                // Значение второй
                UsedRegs ac(this, d->dataType.b());

                // Сохранить регистры, если вызов функции

                compileVar(r->b, 1, 0); //! Можно получить и её адрес, что бы использовать M

                if(t.is8())
                {
                    out.mov(Asm8080::r8_a, Asm8080::r8_m);
                    switch(r->o)
                    {
                        case oSAdd:    out.add(Asm8080::r8_e); break;
                        case oSSub:    out.sub(Asm8080::r8_e); break;
                        case oSAnd:    out.ana(Asm8080::r8_e); break;
                        case oSOr:     out.ora(Asm8080::r8_e); break;
                        case oSXor:    out.xra(Asm8080::r8_e); break;
                        case oSShl:    out.call("__SHL8"); break;
                        case oSShr:    out.call("__SHR8"); break;
                        default: throw;
                    }
                    out.mov(Asm8080::r8_m, Asm8080::r8_a);
                    return;
                }

                if(t.is16())
                {
                    switch(r->o)
                    {
                        case oAdd:     out.dad(Asm8080::r16_de); break;
                        case oSub:     out.call("__SUB16"); break;
                        case oAnd:     out.call("__AND16"); break;
                        case oOr:      out.call("__OR16"); break;
                        case oXor:     out.call("__XOR16"); break;
                        case oShl:     out.call("__SHL16"); break;
                        case oShr:     out.call("__SHR16"); break;
                        default: throw;
                    }
                    return;
                }

                // Сохранение значения
                if(t.is8())
                {
                    out.mov(Asm8080::r8_m, Asm8080::r8_a);
                }
                else if(t.is16())
                {
                    out.xchg();
                    out.mov(Asm8080::r8_m, Asm8080::r8_d);
                    out.dcx(Asm8080::r16_hl);
                    out.mov(Asm8080::r8_m, Asm8080::r8_e);
                }

                throw;
            }

            if(r->o == oShr || r->o==oShl || r->o==oAdd || r->o==oSub || r->o==oAnd || r->o==oOr || r->o==oXor
                 || r->o == oMul || r->o == oDiv || r->o == oMod)
            {
                if(r->a->nodeType == ntConstI && r->b->nodeType != ntConstI)
                    std::swap(r->a, r->b);

                unsigned ur;
                if(d == 1) ur = pushAcc1();
                compileVar(r->a, 0, 0); // В регистр A или HL

                if(t.is16() && r->o == oAdd && r->b->nodeType == ntSP && inStack == 0)
                {
                    out.dad(Asm8080::r16_sp);
                    if(d == 1) popAccSwap(pp, ur);
                    return;
                }

                if(r->b->nodeType == ntConstI && pp == B)
                {
                    unsigned value = r->b->cast<NodeConst>()->value;
                    switch(r->o | pp)
                    {
                        case B | oAdd: out.adi(value); if(d == 1) popAccSwap(pp, ur); break;
                        case B | oSub: out.sui(value); if(d == 1) popAccSwap(pp, ur); break;
                        case B | oAnd: out.ani(value); if(d == 1) popAccSwap(pp, ur); break;
                        case B | oOr:  out.ori(value); if(d == 1) popAccSwap(pp, ur); break;
                        case B | oXor: out.xri(value); if(d == 1) popAccSwap(pp, ur); break;
                        case B | oShl: throw "SHL8CONST";
                        case B | oShr: throw "SHR8CONST";
                    }
                    return;
                }

                UsedRegs ac(this, r->a->dataType.b());

                compileVar(r->b, 1, 0); // В регистр E или DE

                switch(r->o | pp)
                {
                    case B | oAdd:     out.add(Asm8080::r8_e); break;
                    case B | oSub:     out.sub(Asm8080::r8_e); break;
                    case B | oAnd:     out.ana(Asm8080::r8_e); break;
                    case B | oOr:      out.ora(Asm8080::r8_e); break;
                    case B | oXor:     out.xra(Asm8080::r8_e); break;
                    case B | oShl:     out.call("__SHL8"); break;
                    case B | oShr:     out.call("__SHR8"); break;
                    case B | oMul:     out.call("__MUL8"); break;
                    case B | oDiv:     out.call("__DIV8"); break;
                    case B | oMod:     out.call("__MOD8"); break;

                    case W | oAdd:     out.dad(Asm8080::r16_de); break;
                    case W | oSub:     out.call("__SUB16"); break;
                    case W | oAnd:     out.call("__AND16"); break;
                    case W | oOr:      out.call("__OR16"); break;
                    case W | oXor:     out.call("__XOR16"); break;
                    case W | oShl:     out.call("__SHL16"); break;
                    case W | oShr:     out.call("__SHR16"); break;
                    case W | oMul:     out.call("__MUL16"); break;
                    case W | oDiv:     out.call("__DIV16"); break;
                    case W | oMod:     out.call("__MOD16"); break;

                    default: throw std::runtime_error("compile ntOperator cpu");
                }

                if(d == 1) popAccSwap(pf, ur);
                return;
            }

            if(r->o==oE || r->o==oNE || r->o==oL || r->o==oG || r->o==oLE || r->o==oGE)
            {
                unsigned ur;
                if(d == 1) ur = pushAcc1();

                compileVar(r->a, 0, 0); // В регистр A или HL

                if(r->b->nodeType == ntConstI && pf == 'B')
                {
                    out.cpi(r->b->cast<NodeConst>()->value);
                }
                else
                {
                    UsedRegs ac(this, r->a->dataType.b());

                    compileVar(r->b, 1, 0); // В регистр E или DE
                    if(pf=='B') out.cmp(Asm8080::r8_e);
                           else out.call("__CMP16");
                }

                Operator o = r->o;

                unsigned l;
                if(ifOpt)
                {
                    ifOpt->ok = true;
                    if(!ifOpt->ifTrue) o = Tree::inverseOp(o);
                    l = ifOpt->label;
                }
                else
                {
                    l = out.labelsCnt++;
                }

                switch((unsigned)o)
                {
                    case oE:  out.jz(l); break;
                    case oNE: out.jnz(l); break;
                    case oL:  throw; //!!! if(!r->a->dataType.isSigned()) out.cmd(cmdBhi, l); else out.cmd(cmdBgt,l); break;
                    case oG:  throw; //!!! if(!r->a->dataType.isSigned()) out.cmd(cmdBlo, l); else out.cmd(cmdBlt,l); break;
                    case oLE: throw; //!!! if(!r->a->dataType.isSigned()) out.cmd(cmdBhis,l); else out.cmd(cmdBge,l); break;
                    case oGE: throw; //!!! if(!r->a->dataType.isSigned()) out.cmd(cmdBlos,l); else out.cmd(cmdBle,l); break;
                }

                if(ifOpt) return;

                unsigned nextLabel = out.labelsCnt++;
                out.mvi(d ? Asm8080::r8_e : Asm8080::r8_a, 1);
                out.jmp(nextLabel);
                out.addLocalLabel(l);
                out.mvi(d ? Asm8080::r8_e : Asm8080::r8_a, 0);
                out.addLocalLabel(nextLabel);
                if(d == 1) popAcc(ur);

                return;
            }

            throw std::runtime_error("unk operator " + i2s(r->o));
            // oIf
        }

        case ntJmp:
        {
            NodeJmp* j = n->cast<NodeJmp>();
            if(!j->cond)
            {
                out.jmp(j->label->n1);
                return;
            }
            compileJump(j->cond, 0, !j->ifZero, j->label->n1);
            return;
        }

        case ntOperatorIf:
        {
            unsigned falseLabel = out.labelsCnt++;
            unsigned exitLabel = out.labelsCnt++;
            NodeOperatorIf* o = n->cast<NodeOperatorIf>();
            compileJump(o->cond, 0, false, falseLabel);
            compileVar(o->a, d);
            out.jmp(exitLabel);
            out.addLocalLabel(falseLabel);
            compileVar(o->b, d);
            out.addLocalLabel(exitLabel);
            return;
        }

        case ntIf:
        {
            NodeIf* r = n->cast<NodeIf>();
            if(r->f==0 && r->t && r->t->nodeType==ntJmp && r->t->cast<NodeJmp>()->cond==0)
            {
                compileJump(r->cond, 0, true, r->t->cast<NodeJmp>()->label->n1);
                return;
            }

            unsigned falseLabel = out.labelsCnt++;
            compileJump(r->cond, 0, false, falseLabel);
            compileBlock(r->t);
            if(r->f == 0)
            {
                out.addLocalLabel(falseLabel);
            }
            else
            {
                unsigned exitLabel = out.labelsCnt++;
                out.jmp(exitLabel);
                out.addLocalLabel(falseLabel);
                compileBlock(r->f);
                out.addLocalLabel(exitLabel);
            }
            return;
        }

        default:
            throw std::runtime_error("Unknown node " + i2s(n->nodeType));
    }
}

//---------------------------------------------------------------------------------------------------------------------

void Compiler8080::compileBlock(Node* n)
{
    for(; n; n=n->next)
        compileVar(n, 0);
}

//---------------------------------------------------------------------------------------------------------------------

void Compiler8080::compileFunction(Function* f)
{
    if(f->compiled || !f->parsed) return;

    out.c.lstWriter.remark(out.c.out.writePtr, 1, f->name);
    compiler.addLabel(f->name);

    curFn = f;
    if(f->stackSize)
    {
        out.lxi(Asm8080::r16_hl, -f->stackSize);
        out.dad(Asm8080::r16_sp);
        out.sphl();
    }

    //! Вместо двух проходов сделать промежуточный ассемблер

    treePrepare(f);

    out.step0();
    out.labelsCnt = f->labelsCnt;
    returnLabel = out.labelsCnt++;
    compileBlock(f->root);
    out.addLocalLabel(returnLabel);
    // Тут может уже быть RET или JMP сюда же
    if(f->stackSize)
    {
        out.lxi(Asm8080::r16_hl, -f->stackSize); //! Использовать PUSH
        out.dad(Asm8080::r16_sp);
        out.sphl();
    }
    out.ret();

    out.step1();
    out.labelsCnt = f->labelsCnt;
    returnLabel = out.labelsCnt++;
    compileBlock(f->root);
    out.addLocalLabel(returnLabel);
    if(f->stackSize)
    {
	out.xchg();
        out.lxi(Asm8080::r16_hl, f->stackSize); //! Использовать POP
        out.dad(Asm8080::r16_sp);
        out.sphl();
	out.xchg();
    }
    out.ret();

    out.step2();

    curFn = 0;
    f->compiled = true;
}

//---------------------------------------------------------------------------------------------------------------------

void Compiler8080::start(unsigned step)
{
    inStack = 0;
    if(step == 0)
    {
        for(std::list<C::Function>::iterator i=tree.functions.begin(); i!=tree.functions.end(); i++) // МОжно начинать не с начала
            compileFunction(&*i);

        writePtrs.push(this->out.c.out.writePtr);

        for(std::list<C::GlobalVar>::iterator i=tree.globalVars.begin(); i!=tree.globalVars.end(); i++) // МОжно начинать не с начала
        {
            if(!i->compiled && !i->extren1)
            {
                i->compiled = true;
                if(!i->type.is8()) compiler.out.align2(); //!!!! Добавить const char[]
                compiler.addLabel(i->name); //! Вывести предупреждение о дубликате
                if(i->data.size()>0)
                {
                    compiler.out.write(&i->data[0], i->data.size());
                }
            }
        }

        writePtrs.push(this->out.c.out.writePtr);
    }
    else
    {
        if(writePtrs.empty()) throw std::runtime_error("Compiler8080.compile");
        unsigned s = compiler.out.writePtr, e = writePtrs.front();
        writePtrs.pop();

        compiler.disassembly(s,e);

        if(writePtrs.empty()) throw std::runtime_error("Compiler8080.compile");
        compiler.out.writePtr = writePtrs.front();
        writePtrs.pop();
    }
}

//---------------------------------------------------------------------------------------------------------------------

}
