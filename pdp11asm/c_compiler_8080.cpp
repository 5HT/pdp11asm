#include "c_compiler_8080.h"
#include "tools.h"

namespace C {

enum { B=0x10000, W=0x20000, D=0x30000 };

enum { UR_A=1, UR_HL=2 };

//---------------------------------------------------------------------------------------------------------------------

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
// Сохранить используемые регистры в стек

unsigned Compiler8080::pushAcc1()
{
    if(usedRegs & UR_A ) { out.push(Asm8080::r16psw_psw); inStack += 2; }
    if(usedRegs & UR_HL) { out.push(Asm8080::r16psw_hl ); inStack += 2; }
    return usedRegs;
}

//---------------------------------------------------------------------------------------------------------------------
// Восстановить регистры из стека

void Compiler8080::popAcc(unsigned usedRegs)
{
    if(usedRegs & UR_HL) { out.pop(Asm8080::r16psw_hl ); inStack -= 2; }
    if(usedRegs & UR_A ) { out.pop(Asm8080::r16psw_psw); inStack -= 2; }
}

//---------------------------------------------------------------------------------------------------------------------

void Compiler8080::popAccSwap(char d, unsigned usedRegs)
{
    switch(d)
    {
        case 'B':
            // Переместить результат из A в D
            out.mov(Asm8080::r8_e, Asm8080::r8_a);
            break;
        case 'W':
            // Переместить результат из HL в DE
            out.xchg();
            break;
        case 'V':
            break;
        default:
            throw;
    }
    if(usedRegs & UR_HL) { out.pop(Asm8080::r16psw_hl); inStack -= 2; }
    if(usedRegs & UR_A) { out.pop(Asm8080::r16psw_psw); inStack -= 2; }
}

//---------------------------------------------------------------------------------------------------------------------

void Compiler8080::compileJump(NodeVar* v, bool ifTrue, unsigned label)
{
    IfOpt ifOpt;
    ifOpt.ok = false;
    ifOpt.ifTrue = ifTrue;
    ifOpt.label  = label;
    compileVar(v, 0, &ifOpt);
    if(ifOpt.ok) return;

    switch(v->dataType.b())
    {
        case 'B': out.ora(Asm8080::r8_a); break;
        case 'W': out.mov(Asm8080::r8_a, Asm8080::r8_l); out.ora(Asm8080::r8_h); break;
    }

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

void Compiler8080::compileOperatorSet(NodeOperator* r, unsigned d)
{
    if(r->a->nodeType != ntDeaddr) throw;
    NodeDeaddr* aa = r->a->cast<NodeDeaddr>();

    // Сохранение аккумулятора в стеке
    unsigned ur;
    if(d == 1) ur = pushAcc1();

    // Команды процессора работающие с непосредственной константой
    if(aa->var->nodeType == ntConst)
    {
        // Команды процессора сохраняющие значение по непосредственному адресу.
        //! Эта команда должна оставлять значение в стеке!
        NodeConst* bb = aa->var->cast<NodeConst>();

        // Компиляция второго значения в A или HL
        compileVar(r->b, 0, 0);

        // Выполнение операции
        out.mov_pimm_arg1(r->b->dataType.b(), bb->text.c_str(), bb->value);
    }
    else
    if(r->b->nodeType == ntConst)
    {
        NodeConst* c = r->b->cast<NodeConst>();

        // Компиляция первого значения в A или HL
        compileVar(aa->var, 0, 0);

        out.mov_ptr1_imm(r->dataType.b(), c->value, c->text.c_str());
    }
    else
    {
        // Компиляция первого значения в A или HL
        compileVar(aa->var, 0, 0);

        // Компиляция второго значения в D или DE
        UsedRegs ac(this, aa->var->dataType.b());
        compileVar(r->b, 1, 0);

        // Выполнение операции
        out.mov_ptr1_arg2(r->dataType.b());

        // Значение в HL
        if(r->o != oSetVoid) out.xchg();
    }

    // Перенос ARG1->ARG2 и восстановление аккумулятора из стека.
    if(d == 1) popAccSwap(aa->var->dataType.b(), ur);

    return;
}

//---------------------------------------------------------------------------------------------------------------------

void Compiler8080::compileMonoOperatorIncDec(NodeMonoOperator* o, unsigned d)
{
    // Реальный тип
    Type t = o->dataType;
    if(t.addr == 0) throw;
    t.addr--;
    unsigned incSize = t.sizeForInc();

    // Сохранение аккумулятора в стеке
    unsigned ur;
    if(d == 1) ur = pushAcc1();

    // Компиляция адреса в HL
    compileVar(o->a, 0);

    // Выполнение операции
    out.pre_inc(t.b(), o->o, incSize);

    // Перенос ARG1->ARG2 и восстановление аккумулятора из стека
    if(d == 1) popAccSwap('W', ur);

}

//---------------------------------------------------------------------------------------------------------------------

void Compiler8080::compileOperatorAlu(NodeOperator* r, unsigned d)
{
    Type& t = r->b->dataType;
    char pf = t.b();

    // Оптимизация. Для использования команд adi, ani, ori, xri
    bool canSwap = (r->o==oAdd || r->o==oAnd || r->o==oOr || r->o==oXor);
    if(canSwap && r->a->nodeType == ntConst && r->b->nodeType != ntConst) std::swap(r->a, r->b);

    // Сохранение аккумулятора в стеке
    unsigned ur;
    if(d == 1) ur = pushAcc1();

    // Компиляция первого значения в A или HL
    compileVar(r->a, 0, 0);

    // Оптимизация SP
    if(t.is16() && r->o == oAdd && r->b->nodeType == ntSP && inStack == 0)
    {
        out.dad(Asm8080::r16_sp);
        if(d == 1) popAccSwap(pf, ur);
        return;
    }

    // Команды процессора работающие с непосредственной константой
    if(r->b->nodeType == ntConst && pf == 'B')
    {
        NodeConst* nc = r->b->cast<NodeConst>();
        out.alu_byte_arg1_imm(r->o, nc->value, nc->text.c_str());
    }
    else
    if(r->b->nodeType == ntDeaddr && r->b->cast<NodeDeaddr>()->var->isConst() && pf == 'B' && r->o != oShl && r->o != oShr)
    {
        // Команды процессора работающие с непосредственным адресом
        NodeConst* co = r->b->cast<NodeDeaddr>()->var->cast<NodeConst>();
        out.alu_byte_arg1_pimm(r->o, co->value, co->text.c_str());
    }
    else
    {
        // Компиляция второго значения в E или DE
        UsedRegs ac(this, r->a->dataType.b());
        compileVar(r->b, 1, 0);

        // Выполнение операции
        out.alu_arg1_arg2(pf, r->o);
    }

    // Перенос ARG1->ARG2 и восстановление аккумулятора из стека.
    if(d == 1) popAccSwap(pf, ur);
}

//---------------------------------------------------------------------------------------------------------------------

void Compiler8080::compileVar(Node* n, unsigned d, IfOpt* ifOpt)
{
    if(!n->remark.empty())
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

        case ntConst: // Поместить число в аккумулятор
        {        
            NodeConst* c = (NodeConst*)n;
            if(c->text.empty())
            {
                out.mov_argN_imm(c->dataType.b(), d, c->value);
                return;
            }
            if(c->dataType.is8())
            {
                out.mvi(d ? Asm8080::r8_e : Asm8080::r8_a, 0);
                out.c.addFixup(Compiler::ftByte, n->cast<NodeConst>()->text.c_str(), 1);
            }
            else if(c->dataType.is16())
            {
                out.lxi(d ? Asm8080::r16_de : Asm8080::r16_hl, 0, c->text.c_str(), c->uid);
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
            BaseType to_type = c->dataType.addr==0 ? c->dataType.baseType : cbtUShort;
            BaseType from_type = c->var->dataType.addr==0 ? c->var->dataType.baseType : cbtUShort;

            // Оптимизация. Преобразование прямо при чтении из памяти. (uint8_t)*word_ptr;
            if(c->var->nodeType == ntDeaddr && (from_type==cbtShort || from_type==cbtUShort) && (to_type==cbtChar || to_type==cbtUChar))
            {
                NodeDeaddr* da = c->var->cast<NodeDeaddr>();
                if(da->var->nodeType == ntConst)
                {
                    NodeConst* co = da->var->cast<NodeConst>();
                    unsigned ur;
                    if(d==1) ur = pushAcc1(); //! Сохраняет A, когда мы будем портить только HL
                    out.mov_arg1_pimm(c->dataType.b(), co->text.c_str(), co->value);
                    if(d==1) popAccSwap(c->dataType.b(), ur);
                    return;
                }
                unsigned ur;
                if(d==1) ur = pushAcc1(); //! Сохраняет A, когда мы будем портить только HL
                compileVar(da->var, 0, 0); // в HL адрес
                out.mov_argN_ptr1(c->dataType.b(), d); // e,de = [hl]
                if(d==1) popAcc(ur);
                return;
            }

            //! Возможно тут нужно сохранить HL
            compileVar(c->var, d);
            // Преобразование в меньший тип происходит незаметно.
            switch((unsigned)to_type)
            {
                case cbtChar:
                case cbtUChar:
                    switch((unsigned)from_type)
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
                    switch((unsigned)from_type)
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
                default:
                    throw;
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

            // Команды процессора загржающие значение из непосредственного адреса.
            if(c->var->nodeType == ntConst)
            {
                NodeConst* v = c->var->cast<NodeConst>();
                out.mov_arg1_pimm(c->dataType.b(), v->text.c_str(), v->value, v->uid);
                if(d==1) popAccSwap(c->dataType.b(), ur);
            }
            else
            {
                compileVar(c->var, 0, 0); // в HL
                out.mov_argN_ptr1(c->dataType.b(), d); // e,de = [hl]
                if(d==1) popAcc(ur);
            }
            return;
         }

        case ntSwitch: // Оптимизированый выбор
        {
            NodeSwitch* s = n->cast<NodeSwitch>();
            if(s->var) compileVar(s->var, 0);            
            if(s->cases.size() == 0) throw;
            out.xchg();
            unsigned v = 0;
            for(std::map<unsigned int, NodeLabel*>::iterator i=s->cases.begin(); i!=s->cases.end(); i++)
            {
                out.lxi(Asm8080::r16_hl, -i->first);
                out.dad(Asm8080::r16_de);
                out.mov(Asm8080::r8_a, Asm8080::r8_l);
                out.ora(Asm8080::r8_h);
                out.jz(i->second->n1);
            }
            if(s->defaultLabel == 0) throw std::runtime_error("NodeSwitch->defaultLabel");
            out.jmp(s->defaultLabel->n1);
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
                compileMonoOperatorIncDec(o, d);
                return;
            }

            if(o->o==moIncVoid || o->o==moDecVoid)
            {
                assert(d == 0);

                // Реальный тип
                Type t = o->dataType;
                if(t.addr == 0) throw;
                t.addr--;
                unsigned incSize = t.sizeForInc();

                if(o->a->isConst() && incSize<=2)
                {
                    NodeConst* co = o->a->cast<NodeConst>();
                    out.inc_pimm(t.b(), o->o==moIncVoid ? incSize : -incSize, co->value, co->text.c_str(), co->uid);
                }
                else
                {
                    compileVar(o->a, 0);
                    out.post_inc(t.b(), o->o, incSize, d);
                }
                return;
            }

            if(o->o==moPostInc || o->o==moPostDec) // На входе адрес, на выходе значение
            {
                // Реальный тип
                unsigned incSize = o->dataType.sizeForInc();

                // Ссылка в значениях
                if(o->a->nodeType != ntDeaddr) throw std::runtime_error("monoOperator !deaddr (%u)" + i2s(o->a->nodeType));

                // Сохранение аккумулятора в стеке
                unsigned ur;
                if(d) ur = pushAcc1();

                // Компиляция адреса в HL
                compileVar(o->a->cast<NodeDeaddr>()->var, 0);

                // Выполнение операции
                bool needSwap = out.post_inc(o->dataType.b(), o->o, incSize, d);

                // Перенос ARG1->ARG2 и восстановление аккумулятора из стека
                if(d) popAccSwap(needSwap ? o->a->dataType.b() : 'V', ur);

                return;
            }

            char pf;
            unsigned pp;
            if(o->a->dataType.is8()) pf='B', pp=B; else
            if(o->a->dataType.is16()) pf='W', pp=W; else throw std::runtime_error("monoOperator !stack");

            if(o->o==moNot && ifOpt)
            {
                if(d) throw;
                compileJump(o->a, !ifOpt->ifTrue, ifOpt->label);
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
            char pf = t.b();

            if(r->o==oLAnd || r->o==oLOr)
            {
                if(ifOpt)
                {
                    if(d) throw;
                    ifOpt->ok = true;
                    unsigned nextLabel = out.labelsCnt++;
                    if(r->o==oLAnd)
                    {
                        if(ifOpt->ifTrue)
                        {
                            compileJump(r->a, false, nextLabel);
                            compileJump(r->b, true, ifOpt->label);
                        }
                        else
                        {
                            compileJump(r->a, false, ifOpt->label);
                            compileJump(r->b, false, ifOpt->label);
                        }
                    }
                    else // oLOr
                    {
                        if(ifOpt->ifTrue)
                        {
                            compileJump(r->a, true, ifOpt->label);
                            compileJump(r->b, true, ifOpt->label);
                        }
                        else
                        {
                            compileJump(r->a, true, nextLabel);
                            compileJump(r->b, false, ifOpt->label);
                        }
                    }
                    out.addLocalLabel(nextLabel);
                    return;
                }

                unsigned trueLabel = out.labelsCnt++;
                unsigned falseLabel = out.labelsCnt++;
                unsigned nextLabel = out.labelsCnt++;

                unsigned ur;
                if(d) ur = pushAcc1();

                if(r->o==oLAnd)
                {
                    compileJump(r->a, false, falseLabel);
                    compileJump(r->b, false, falseLabel);
                }
                else
                {
                    compileJump(r->a, true,  trueLabel);
                    compileJump(r->b, false, falseLabel);
                }

                out.addLocalLabel(trueLabel);
                out.mvi(d ? Asm8080::r8_e : Asm8080::r8_d, 1);
                out.jmp(nextLabel);

                out.addLocalLabel(falseLabel);
                out.mvi(d ? Asm8080::r8_e : Asm8080::r8_d, 0);

                out.addLocalLabel(nextLabel);

                if(d) popAcc(ur);
                return;
            }

            if(r->o==oSetVoid || r->o==oSet)
            {
                compileOperatorSet(r, d);
                return;
            }

            if(r->o==oSShr || r->o==oSShl || r->o==oSAdd || r->o==oSSub || r->o==oSAnd || r->o==oSOr || r->o==oSXor)
            {
                // Ссылка в значениях
                if(r->a->nodeType != ntDeaddr) throw std::runtime_error("monoOperator !deaddr (%u)" + i2s(r->a->nodeType));
                NodeDeaddr* de = r->a->cast<NodeDeaddr>();
                NodeVar* a1 = de->var;

                //! Поместить результат в D!
                //! Вообще не помещать результат

                if(a1->isConst())
                {
                    NodeConst* co = a1->cast<NodeConst>();
                    out.mov_arg1_pimm(t.b(), co->text.c_str(), co->value, co->uid);
                }
                else
                {
                    // Адрес первой переменной
                    compileVar(a1, 0, 0);
                    // Сохраняем адрес в стеке и получаем значение
                    if(t.is16())
                    {
                        out.push(Asm8080::r16psw_hl);
                        out.mov_argN_ptr1(t.b(), 0);
                    }
                }

                // Значение второй переменной
                UsedRegs ac(this, 'W');
                compileVar(r->b, 1, 0);

                if(t.is8())
                {
                    out.mov(Asm8080::r8_a, Asm8080::r8_m);
                }

                out.alu_arg1_arg2(t.b(), r->o);

                if(a1->isConst())
                {
                    NodeConst* co = a1->cast<NodeConst>();
                    out.mov_pimm_arg1(t.b(), co->text.c_str(), co->value, co->uid);
                }
                else
                if(pf=='B')
                {
                    out.mov(Asm8080::r8_m, Asm8080::r8_a);
                }
                else
                {
                    out.pop(Asm8080::r16psw_de);
                    out.xchg();
                    out.mov(Asm8080::r8_m, Asm8080::r8_d);
                    out.dcx(Asm8080::r16_hl);
                    out.mov(Asm8080::r8_m, Asm8080::r8_e);
                }

                return;
            }

            if(r->o == oShr || r->o==oShl || r->o==oAdd || r->o==oSub || r->o==oAnd || r->o==oOr || r->o==oXor || r->o == oMul || r->o == oDiv || r->o == oMod)
            {
                compileOperatorAlu(r, d);
                return;
            }


            if(r->o==oE || r->o==oNE || r->o==oL || r->o==oG || r->o==oLE || r->o==oGE)
            {
                // Оптимизация. Для использования команды cpi, lxi d
                if(r->a->nodeType == ntConst && r->b->nodeType != ntConst)
                {
                    std::swap(r->a, r->b);
                    switch((unsigned)r->o) {
                        case oL:  r->o = oG;  break;
                        case oLE: r->o = oGE; break;
                        case oG:  r->o = oL;  break;
                        case oGE: r->o = oLE; break;
                    }
                }

                // Оптимизация. Замена пары команд условного перехода на одну
                if(r->b->isConstI())
                {
                    unsigned& v = r->b->cast<NodeConst>()->value;
                    if(v < (pf=='B' ? 0xFF : 0xFFFF))
                    {
                        switch((unsigned)r->o) {
                            case oG: r->o = oGE; v++; break;
                            case oLE: r->o = oL; v++; break;
                        }
                    }
                }

                // Сохранение аккумулятора в стеке
                unsigned ur;
                if(d == 1) ur = pushAcc1();

                // Компиляция первого значения в A или HL
                compileVar(r->a, 0, 0);

                // Команды процессора работающие с непосредственной константой
                if(r->b->nodeType == ntConst && pf == 'B')
                {
                    out.cpi((unsigned)r->o, r->b->cast<NodeConst>()->value, r->b->cast<NodeConst>()->text.c_str());
                }
                else
                {
                    // Компиляция второго значения в E или DE
                    UsedRegs ac(this, r->a->dataType.b());
                    compileVar(r->b, 1, 0); // В регистр E или DE

                    // Выполнение операции
                    if(pf=='B') out.cmp(Asm8080::r8_e);
                           else out.call("__CMP16");
                }

                if(ifOpt)
                {
                    ifOpt->ok = true;
                    Operator o = r->o;
                    if(!ifOpt->ifTrue) o = Tree::inverseOp(o);
                    if(d==1) throw;
                    //!!!!!!!!!!!!! снесёт флаги if(d == 1) popAcc(ur);
                    out.jmp_cc((unsigned)o, r->a->dataType.isSigned(), ifOpt->label);
                }
                else
                {
                    unsigned l = out.labelsCnt++;
                    out.jmp_cc((unsigned)r->o, r->a->dataType.isSigned(), l);

                    // Возвращаемое значение 0 или 1
                    unsigned nextLabel = out.labelsCnt++;
                    Asm8080::r8 output_reg = d ? Asm8080::r8_e : Asm8080::r8_a;
                    out.mvi(output_reg, 1);
                    out.jmp(nextLabel);

                    out.addLocalLabel(l);
                    out.mvi(output_reg, 0);
                    out.addLocalLabel(nextLabel);

                    if(d == 1) popAcc(ur);
                }

                return;
            }

            throw std::runtime_error("unk operator " + i2s(r->o));
            // oIf
        }

        case ntJmp:
        {
            NodeJmp* j = n->cast<NodeJmp>();
            if(j->cond) compileJump(j->cond, !j->ifZero, j->label->n1);
                   else out.jmp(j->label->n1);
            return;
        }

        case ntOperatorIf:
        {
            unsigned falseLabel = out.labelsCnt++;
            unsigned exitLabel = out.labelsCnt++;
            NodeOperatorIf* o = n->cast<NodeOperatorIf>();
            compileJump(o->cond, false, falseLabel);
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
                compileJump(r->cond, true, r->t->cast<NodeJmp>()->label->n1);
                return;
            }

            unsigned falseLabel = out.labelsCnt++;
            compileJump(r->cond, false, falseLabel);
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
// Компиляция отдельной функции

void Compiler8080::compileFunction(Function* f)
{
    // Выходим если уже скомпилирована. Или выходим, имеется только прототип функции
    if(f->compiled || !f->parsed) return;

    // Записать в LST файл комментарий - имя функции
    out.c.lstWriter.remark(out.c.out.writePtr, 1, f->name);

    // Сохраняем адрес функции как метку
    compiler.addLabel(f->name);

    // Указатель на компилируемую функцию
    curFn = f;

    // Если используются стековые переменные, то резервируем под них место
    if(f->stackSize)
    {
        out.lxi(Asm8080::r16_hl, -f->stackSize);
        out.dad(Asm8080::r16_sp);
        out.sphl();
    }

    // Рассчитать идентификаторы меток
    treePrepare(f);

    // Первый свободный идентификатор метки
    out.labelsCnt = f->labelsCnt;

    // Идентификатор метки выхода из функции
    returnLabel = out.labelsCnt++;

    // Компиляция функции
    compileBlock(f->root);

    // Точка выхода из функции
    out.addLocalLabel(returnLabel);
    
    // Если используются стековые переменные, то освобождаем стек.
    // В HL может находится возвращаемое значение.
    if(f->stackSize)
    {
        if(f->retType.is16()) out.xchg();
        out.lxi(Asm8080::r16_hl, f->stackSize); //! Использовать POP
        out.dad(Asm8080::r16_sp);
        out.sphl();
        if(f->retType.is16()) out.xchg();
    }

    // Выход из функции
    out.ret();

    // Расставить адреса переходов
    out.setJumpAddresses();

    // Обнуляем указатель на компилируемую функцию. На всякий случай.
    curFn = 0;

    // Функция скомпилирована
    f->compiled = true;
}

//---------------------------------------------------------------------------------------------------------------------
// Начало компиляции

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
