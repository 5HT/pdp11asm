// PDP11 Assembler (c) 08-01-2017 ALeksey Morozov (aleksey.f.morozov@gmail.com)

// 1) Если мы что то занесли в стек, то скорретировать SP
// 2) Глобальные переменные
// 3) ADD #2, SP заменить на PUSH
// 4) RET на BR/JMP
// 5) FAR JUMP
// 6) Операторы &= ^= |= +=
// 7) Убрать лишние TST

#include "c_compiler_pdp11.h"
#include "tools.h"

namespace C {

enum { B=0x10000, W=0x20000, D=0x30000 };

//---------------------------------------------------------------------------------------------------------------------

CompilerPdp11::CompilerPdp11(Compiler& _compiler, Tree& _tree) : out(_compiler), compiler(_compiler), tree(_tree)
{
    curFn = 0;
}

//---------------------------------------------------------------------------------------------------------------------
// Сохранить аккумулятор

void CompilerPdp11::pushAcc(char pf)
{
    out.push(Arg11::r0); inStack += 2;
    if(pf=='D') { out.push(Arg11::r2); inStack += 2; }
}

//---------------------------------------------------------------------------------------------------------------------

void CompilerPdp11::popAcc(char pf)
{
    if(pf=='D') { out.pop(Arg11::r2); inStack -= 2; }
    out.pop(Arg11::r0); inStack -= 2;
}

//---------------------------------------------------------------------------------------------------------------------

void CompilerPdp11::popAcc_A2D(char pf)
{
    out.cmd(cmdMov, Arg11::r0, Arg11::r1);
    out.pop(Arg11::r0);
    inStack -= 2;
    if(pf=='D')
    {
        out.cmd(cmdMov, Arg11::r2, Arg11::r3);
        out.pop(Arg11::r2);
        inStack -= 2;
    }
}

//---------------------------------------------------------------------------------------------------------------------

bool CompilerPdp11::compileArg(bool check, Arg11& ol, Arg11& oh, NodeVar* a, int d, int dr, char pf)
{
    //! Игнор в том числе signed!
    while(a->nodeType == ntConvert && a->dataType.addr!=0 && ((NodeConvert*)a)->var->dataType.addr!=0)
        a = ((NodeConvert*)a)->var;

    // SP можно разметить в аргументе
    if(a->nodeType == ntSP && inStack == 0)
    {
        ol = Arg11::sp;
        oh = Arg11::null;
        return false;
    }

    // Число можно разместить в аргументе
    if(a->nodeType == ntConst)
    {
        if(a->cast<NodeConst>()->text.empty())
        {
            unsigned v = a->cast<NodeConst>()->value;
            ol = Arg11(atValue, 0, v & 0xFFFF);
            oh = Arg11(atValue, 0, v >> 16);
            return false;
        }
        ol.type = atValue;
        ol.str = a->cast<NodeConst>()->text;
        ol.value = 0;        
        oh = Arg11::null;
        return false;
    }

    // Адрес можно разместить в аргументе
    if(a->nodeType == ntDeaddr)
    {
        NodeVar* b = a->cast<NodeDeaddr>()->var;

        //!!!! Только не влияющие на Ассемблер
        while(b->nodeType == ntConvert && b->dataType.addr!=0 && ((NodeConvert*)b)->var->dataType.addr!=0)
            b = ((NodeConvert*)b)->var;

        // Оптимизация Deaddr(Const) одной командой
        if(b->nodeType == ntConst)
        {
            if(b->cast<NodeConst>()->text.empty())
            {
                ol.type = atValueMem;
                ol.value = b->cast<NodeConst>()->value;
                oh.type = atValueMem;
                oh.value = b->cast<NodeConst>()->value + 2;
                return false;
            }
            ol.type = atValueMem;
            ol.value = 0;
            ol.str = b->cast<NodeConst>()->text;
            oh.type = atValueMem;
            oh.str = b->cast<NodeConst>()->text + "+2"; //!!!!!!!!!!!!! Это косяк
            return false;
        }

        bool dbl = false;
        if(b->nodeType == ntDeaddr)
        {
            b = b->cast<NodeDeaddr>()->var;
            dbl = true;
        }

        //!!!! Только не влияющие на Ассемблер
        while(b->nodeType == ntConvert && b->dataType.addr!=0 && ((NodeConvert*)b)->var->dataType.addr!=0)
            b = ((NodeConvert*)b)->var;

        if(b->nodeType == ntOperator)
        {
            NodeOperator* e = b->cast<NodeOperator>();
            if(e->o == oAdd)
            {
                if(e->a->isNotConstI() && e->b->isConstI()) std::swap(e->a, e->b);
                if(e->a->isConstI())
                {
                    int off = e->a->cast<NodeConst>()->value;
                    if(e->b->nodeType == ntSP) // Оптимизация Deaddr(Add(Const,SP)) одной командой
                    {
                        ol.type = dbl ? atRegValueMemMem : atRegValueMem;
                        ol.reg  = 6;
                        ol.value = off + inStack;
                        oh.type = ol.type;
                        oh.reg  = 6;
                        oh.value = off + inStack + 2;
                        //printf("olv=%u\n", ol.value);
                        return false;
                    }
                    // Оптимизация Deaddr(Add(Const,REG)) одной командой
                    if(check) return true;
                    if(d==0 && dr==1) pushAcc(pf);
                    compileVar(e->b, d);
                    ol.type = dbl ? atRegValueMemMem : atRegValueMem;
                    ol.reg  = d;
                    ol.value = off;
                    oh.type = ol.type;
                    oh.reg  = d;
                    oh.value = off+2;
                    return (d==0 && dr==1);
                }
            }
        }

        // Оптимизация Deaddr(SP) одной командой
        if(b->nodeType == ntSP)
        {
            ol.type = dbl ? atRegValueMemMem : atRegMem;
            ol.reg = 6;
            ol.value = inStack;

            oh.type = ol.type;
            oh.value = inStack + 2;
            oh.reg = 6;
            return false;
        }

        //! Двойной адрес

        // Оптимизация Deaddr(REG) одной командой
        if(check) return true;
        if(d==0 && dr==1) pushAcc(pf);
        compileVar(b, d);
        ol.type  = dbl ? atRegValueMemMem : atRegMem;
        ol.reg   = d;
        ol.value = 0;
        oh.type  = ol.type;
        oh.reg   = d;
        oh.value = 2;
        return (d==0 && dr==1);
    }

    // Без оптимизации
    if(check) return true;
    if(d==0 && dr==1) pushAcc(pf);
    compileVar(a, d);
    ol.type = atReg;
    ol.reg  = d;
    oh.type = atReg;
    oh.reg  = d+2;
    return (d==0 && dr==1);
}

//---------------------------------------------------------------------------------------------------------------------

void CompilerPdp11::compileJump(NodeVar* v, int d, bool ifTrue, unsigned label)
{
    IfOpt ifOpt;
    ifOpt.ok = false;
    ifOpt.ifTrue = ifTrue;
    ifOpt.label  = label;
    compileVar(v, d, &ifOpt);
    if(ifOpt.ok) return;

    out.cmd(cmdTst, Arg11(atReg, d));
    if(ifTrue) out.cmd(cmdBne, label);
         else  out.cmd(cmdBeq, label);
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

bool CompilerPdp11::compileDualArg(Arg11& al, Arg11& ah, Arg11& bl, Arg11& bh, bool s, unsigned d, NodeOperator* r, char pf)
{
    bool pop = false;
    bool firstRegUsed  = compileArg(true, al, ah, r->a, 0, 0, pf);
    bool secondRegUsed = compileArg(true, bl, bh, r->b, 0, 0, pf);

    // Аргумент, который мы собираемся поместить во временный регистр полностью помещается в аргумент.
    // А тот, что планировали поместить в аргумент все равно использует временный регистр. Выгоднее поменять местами.

    if(out.step==0 && !firstRegUsed && secondRegUsed)
    {
        if(r->o == oAdd || r->o == oOr || r->o == oXor )
        {
            std::swap(secondRegUsed, firstRegUsed);
            std::swap(r->a, r->b);
        }
    }

    unsigned d1 = d==1 && !secondRegUsed ? 1 : 0; // Если для вычисления требуется всего один регистр и результат должен быть в B, то сразу в B
    if(s)
    {
        pop = compileArg(false, al, ah, r->a, d1, d, pf); //! для oSet аккумулятор долежн содержать устанаввиваемое значение
    }
    else
    {
        if(d1==0 && d==1)
        {
            pushAcc(pf);
            pop = true;
        }
        compileVar(r->a, d1);
        al.type = atReg;
        al.reg  = d1;
        ah.type = atReg;
        ah.reg  = d1+2;
    }
    compileArg(false, bl, bh, r->b, d==0 && !al.regUsed() ? 0 : 1, d, pf); // Если R0 не занят, занять его!
    return pop;
}

//---------------------------------------------------------------------------------------------------------------------

void CompilerPdp11::compileVar(Node* n, unsigned d, IfOpt* ifOpt)
{
    if(!n->remark.empty() && out.step==1)
        out.c.lstWriter.remark(out.c.out.writePtr, 0, n->remark.c_str());

    switch(n->nodeType)
    {
        case ntLabel: // Метка
            out.addLocalLabel(n->cast<NodeLabel>()->n1);
            return;

        case ntSP: // Поместить SP в аккумулятор
            out.cmd(cmdMov, Arg11::sp, Arg11(atReg, d));
            if(inStack) out.cmd(cmdAdd, Arg11(atValue, 0, inStack), Arg11::sp);
            return;

        case ntConst: // Числовая константа
        {
            if(!n->cast<NodeConst>()->text.empty())
            {
                out.cmd(cmdMov, Arg11::null, Arg11(atReg, d));
                if(out.step==1) out.c.addFixup(Compiler::ftWord, n->cast<NodeConst>()->text.c_str(), 2);
                return;
            }

            NodeConst* c = (NodeConst*)n;
            char pf;
            if(c->dataType.is8()) pf='B'; else
            if(c->dataType.is16()) pf='W'; else
            if(c->dataType.is32()) pf='D'; else throw std::runtime_error("monoOperator !stack");

            Arg11 al, ah;
            compileArg(false, al, ah, c, d, d, pf);

            out.cmd(cmdMov, al, Arg11(atReg, d));
            if(pf=='D') out.cmd(cmdMov, ah, Arg11(atReg, d+2));
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
                case cbtShort:
                case cbtUShort:
                    switch((unsigned)f)
                    {
                        case cbtChar:   out.call(d ? "SGNB1" : "SGNB0"); return;
                        case cbtUChar:  out.cmd(cmdBic, Arg11(atValue, 0, 0xFF00), Arg11(atReg, d)); return;
                    }
                    return;
                case cbtLong:
                case cbtULong:
                    switch((unsigned)f)
                    {
                        case cbtChar:   out.call(d ? "SGNB1" : "SGNB0"); out.call(d ? "SGNW1" : "SGNW0"); return;
                        case cbtUChar:  out.cmd(cmdBic, Arg11(atValue, 0, 0xFF00), Arg11(atReg, d)); out.cmd(cmdClr, Arg11(atReg, d+2)); return;
                        case cbtShort:  out.call(d ? "SGNW1" : "SGNW0"); return;
                        case cbtUShort: out.cmd(cmdClr, Arg11(atReg, d+2)); return;
                    }
            }
            return;
        }

        case ntCall: // Вызов функции
        {
            NodeCall* c = n->cast<NodeCall>();

            if(d==1) pushAcc('D');

            unsigned ss = 0;
            unsigned regs_founded = 0;
            for(std::vector<FunctionArg>::reverse_iterator i = c->f->args.rbegin(); i != c->f->args.rend(); i++)
            {
                if(i->n >= c->args1.size()) throw std::runtime_error("ntCall args"); // Антибаг
                if(i->reg) { regs_founded++; continue; }
                NodeVar* v = c->args1[i->n];
                Arg11 al,ah;
                compileArg(false, al,ah,v,0,0,bwd(v->dataType));
                if(v->dataType.is8()) { ss+=2; inStack+=2; out.pushb(al); } else //! Может в PUSH затолкнуть?
                if(v->dataType.is16()) { ss+=2; inStack+=2; out.push(al); } else
                if(v->dataType.is32()) { ss+=4; inStack+=4; out.push(ah); out.push(al); } else throw std::runtime_error("call !stack");
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
                    Arg11 al,ah;
                    compileArg(false, al,ah,v,0,0,bwd(v->dataType));
                    if(!v->dataType.is8_16()) throw std::runtime_error("call !stack");
                    if(regs_founded==0) out.cmd(v->dataType.is8() ? cmdMovb : cmdMov, al, Arg11(atReg, i->reg-1)); else { ss+=2; inStack+=2; out.push(al); }
                }

                for(std::vector<FunctionArg>::iterator i = c->f->args.begin(); i != c->f->args.end(); i++, n++)
                {
                    if(i->reg)
                    {
                        if(regs_founded) { out.pop(Arg11(atReg, i->reg - 1)); ss-=2; inStack-=2; }
                        regs_founded++;
                    }
                }
            }

            switch(c->f->call_type)
            {
                case 0: out.call(c->name.c_str(), c->addr); break;
                case 1: out.emt(c->f->call_arg); break;
                default: throw std::runtime_error("ntCall !reg_type");
            }

            if(ss) out.cmd(cmdAdd, Arg11(atValue, 0, ss), Arg11::sp);
            inStack-=ss;

            if(d==1)
            {
                if(c->f->reg != 2) out.cmd(cmdMov, c->f->reg ? Arg11(atReg, c->f->reg-1) : Arg11::r0, Arg11::r1);
                if(c->dataType.is32()) out.cmd(cmdMov, Arg11::r2, Arg11::r3); //! EMT вызовов 32 бита не бывает
                popAcc('D');
                inStack-=2;
            }
            else
            {
                if(c->f->reg != 0 && c->f->reg != 1) out.cmd(cmdMov, Arg11(atReg, c->f->reg-1), Arg11::r0);
            }

            //! Освободить стек
            return;
        }

        case ntDeaddr: // Получить значение по адресу
        {
            NodeDeaddr* c = (NodeDeaddr*)n;

            //! Заменить на этот вызов
            char pf;
            if(c->dataType.is8()) pf='B'; else
            if(c->dataType.is16()) pf='W'; else
            if(c->dataType.is32()) pf='D'; else throw std::runtime_error("monoOperator !stack");

            Arg11 al, ah;
            compileArg(false, al, ah, c, d, d, pf);

            // Результат уже в нужном регистре
            if(al.type != atReg || al.reg != d) out.cmd(pf=='B' ? cmdMovb : cmdMov, al, Arg11(atReg, d));
            if(pf == 'D') if(ah.type != atReg || ah.reg != d+2) out.cmd(cmdMov, ah, Arg11(atReg, d+2));

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
            NodeReturn* r = n->cast<NodeReturn>();
            if(d != 0) throw std::runtime_error("return !d");
            if(r->var) compileVar(r->var, 0);            
            if(curFn->stackSize != 0) out.cmd(cmdBr, returnLabel); else out.ret();
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
                if(t.is16()) pp=W; else
                if(t.is32()) pp=D; else throw std::runtime_error("monoOperator inc/dec !stack");
                unsigned s = t.sizeForInc();

                // D - содержит адрес
                compileVar(o->a, d);

                switch(o->o | pp)
                {
                    case moInc | B: if(s==1) out.cmd(cmdIncb, Arg11(atRegMem, d)); else out.cmd(cmdAdd, Arg11(atValue, 0, s), Arg11(atRegMem, d)); return;
                    case moDec | B: if(s==1) out.cmd(cmdDecb, Arg11(atRegMem, d)); else out.cmd(cmdSub, Arg11(atValue, 0, s), Arg11(atRegMem, d)); return;
                    case moInc | W: if(s==1) out.cmd(cmdInc,  Arg11(atRegMem, d)); else out.cmd(cmdAdd, Arg11(atValue, 0, s), Arg11(atRegMem, d)); return;
                    case moDec | W: if(s==1) out.cmd(cmdDec,  Arg11(atRegMem, d)); else out.cmd(cmdSub, Arg11(atValue, 0, s), Arg11(atRegMem, d)); return;
                    case moInc | D: out.cmd(cmdAdd, Arg11(atValue, 0, s), Arg11(atRegMem, d)); out.cmd(cmdAdc, Arg11(atRegValueMem,d,2)); return;
                    case moDec | D: out.cmd(cmdSub, Arg11(atValue, 0, s), Arg11(atRegMem, d)); out.cmd(cmdSbc, Arg11(atRegValueMem,d,2)); return;
                }

                throw std::runtime_error("int");
            }

            char pf;
            unsigned pp;
            if(o->a->dataType.is8()) pf='B', pp=B; else
            if(o->a->dataType.is16()) pf='W', pp=W; else
            if(o->a->dataType.is32()) pf='D', pp=D; else throw std::runtime_error("monoOperator !stack");
            unsigned s = o->a->dataType.sizeForInc();

            if(o->o==moIncVoid || o->o==moDecVoid) // На входе адрес, на выходе значение
            {
                Arg11 al, ah;
                compileArg(false, al, ah, o->a, d, d, pf);
                switch(o->o | pp)
                {
                    case B | moIncVoid: if(s==1) out.cmd(cmdIncb, al); else out.cmd(cmdAdd, Arg11(atValue, 0, s), al); return;
                    case B | moDecVoid: if(s==1) out.cmd(cmdDecb, al); else out.cmd(cmdSub, Arg11(atValue, 0, s), al); return;
                    case W | moIncVoid: if(s==1) out.cmd(cmdInc,  al); else out.cmd(cmdAdd, Arg11(atValue, 0, s), al); return;
                    case W | moDecVoid: if(s==1) out.cmd(cmdDec,  al); else out.cmd(cmdSub, Arg11(atValue, 0, s), al); return;
                    case D | moIncVoid: out.cmd(cmdAdd, Arg11(atValue, 0, s), al); out.cmd(cmdAdc, ah); return;
                    case D | moDecVoid: out.cmd(cmdSub, Arg11(atValue, 0, s), al); out.cmd(cmdSbc, ah); return;
                }
                throw std::runtime_error("int");
            }

            if(o->o==moPostInc || o->o==moPostDec) // На входе адрес, на выходе значение
            {
                // Ссылка в значениях
                if(o->a->nodeType != ntDeaddr) throw std::runtime_error("monoOperator !deaddr (%u)" + i2s(o->a->nodeType));

                Arg11 al, ah;
                compileArg(false, al, ah, o->a, d, d, pf);
                bool au = al.regUsed();

                if(pf=='B')
                {
                    out.cmd(cmdMovb, al, au ? Arg11::r4 : Arg11(atReg, d));
                    if(s==1) out.cmd(o->o==moPostInc ? cmdIncb : cmdDecb, al);
                        else out.cmd(o->o==moPostInc ? cmdAdd : cmdSub, Arg11(atValue, 0, s), al);
                    if(au) out.cmd(cmdMovb, Arg11::r4, Arg11(atReg, d));
                    return;
                }

                if(pf=='W')
                {
                    out.cmd(cmdMov, al, au ? Arg11::r4 : Arg11(atReg, d));
                    if(s==1) out.cmd(o->o==moPostInc ? cmdInc : cmdDec, al);
                        else out.cmd(o->o==moPostInc ? cmdAdd : cmdSub, Arg11(atValue, 0, s), al);
                    if(au) out.cmd(cmdMov, Arg11::r4, Arg11(atReg, d));
                    return;
                }

                if(pf=='D')
                {
                    out.cmd(cmdMov, al, au ? Arg11::r4 : Arg11(atReg, d));
                    out.cmd(cmdMov, ah, Arg11(atReg, d+2));
                    out.cmd(o->o==moPostInc ? cmdAdd : cmdSub, Arg11(atValue, 0, s), al);
                    out.cmd(o->o==moPostInc ? cmdAdc : cmdSbc, ah);
                    if(au) out.cmd(cmdMov, Arg11::r4, Arg11(atReg, d));
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
                compileVar(o->a, d);
                switch(o->o | pp)
                {
                    case moNot | B: out.call("NOTB"); return;
                    case moNeg | B: out.cmd(cmdNeg, Arg11(atReg, d)); return;
                    case moXor | B: out.cmd(cmdCom, Arg11(atReg, d)); return;
                    case moNot | W: out.call("NOTW"); return;
                    case moNeg | W: out.cmd(cmdNeg, Arg11(atReg, d)); return;
                    case moXor | W: out.cmd(cmdCom, Arg11(atReg, d)); return;
                    case moNot | D: out.call("NOTD"); return;
                    case moNeg | D: out.call("NEGD"); return;
                    case moXor | D: out.cmd(cmdCom, Arg11(atReg, d)); out.cmd(cmdCom, Arg11(atReg, d+2)); return;
                }
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
                out.cmd(cmdMov, Arg11(atValue, 0, 1), Arg11(atReg, d));
                out.cmd(cmdBr, nextLabel);

                out.addLocalLabel(falseLabel);
                out.cmd(cmdClr, Arg11(atReg, d));

                out.addLocalLabel(nextLabel);
                return;
            }

            if(((r->o == oShr || r->o==oShl || r->o == oSShr || r->o==oSShl) && can_shift(r)) ||  r->o==oAdd || r->o==oSub || r->o==oAnd || r->o==oOr || r->o==oXor || r->o==oSAdd || r->o==oSSub || r->o==oSAnd || r->o==oSOr || r->o==oSXor || r->o==oSetVoid || r->o==oSet)
            {
                bool s = r->o==oSAdd || r->o==oSSub || r->o==oSAnd || r->o==oSOr || r->o==oSXor || r->o==oSetVoid || r->o == oSShr || r->o==oSShl || r->o==oSet;

                Arg11 al, ah, bl, bh;
                bool pop = compileDualArg(al, ah, bl, bh, s, d, r, pf);

                switch(r->o | pp)
                {
                    case B | oSAdd:    if(bl.type == atValue && bl.value == 1 && bl.str.empty()) { out.cmd(cmdIncb, al); break; }
                                       if(bl.type == atValue && bl.value == 2 && bl.str.empty()) { out.cmd(cmdIncb, al); out.cmd(cmdIncb, al); break; }
                                       //! Реализовать 8 битное сложение +=
                    case B | oAdd:     throw std::runtime_error("ADD8!");
                    case B | oSSub:
                    case B | oSub:     throw std::runtime_error("SUB8!");
                    case B | oSAnd:
                    case B | oAnd:     out.cmd(cmdBicb, bl, al); break; //!!! AND нет
                    case B | oSOr:
                    case B | oOr:      out.cmd(cmdBisb, bl, al); break;
                    case B | oSXor:
                    case B | oXor:     if(bl.type==atReg) out.xor_r_a(bl.reg, al); else { out.cmd(cmdMov, bl, Arg11::r4); out.xor_r_a(4, al); } break; //!!! нет 8 бит
                    case B | oSet:
                    case B | oSetVoid: out.cmd(cmdMovb, bl, al); break;
                    case B | oShl:     shift(al, ah, bl, true, pf); break;
                    case B | oShr:     shift(al, ah, bl, false, pf); break;

                    case W | oSAdd:
                    case W | oAdd:     out.cmd(cmdAdd, bl, al); break;
                    case W | oSSub:
                    case W | oSub:     out.cmd(cmdSub, bl, al); break;
                    case W | oSAnd:
                    case W | oAnd:     out.cmd(cmdBic, bl, al); break; //!!! AND нет
                    case W | oSOr:
                    case W | oOr:      out.cmd(cmdBis, bl, al); break;
                    case W | oSXor:
                    case W | oXor:     if(bl.type==atReg) out.xor_r_a(bl.reg, al); else { out.cmd(cmdMov, bl, Arg11::r4); out.xor_r_a(4, al); } break; //! Можно XOR оптимизировать
                    case W | oSet:
                    case W | oSetVoid: out.cmd(cmdMov, bl, al); break;
                    case W | oShl:     shift(al, ah, bl, true, pf); break;
                    case W | oShr:     shift(al, ah, bl, false, pf); break;

                    case D | oSAdd:
                    case D | oAdd:     out.cmd(cmdAdd, bl, al); out.cmd(cmdAdc, ah); out.cmd(cmdAdd, bh, ah); break;
                    case D | oSSub:
                    case D | oSub:     out.cmd(cmdSub, bl, al); out.cmd(cmdSbc, ah); out.cmd(cmdSub, bh, ah); break;
                    case D | oSAnd:
                    case D | oAnd:     out.cmd(cmdBic, bl, al); out.cmd(cmdBic, bl, al); break; //!!! AND нет
                    case D | oSOr:
                    case D | oOr:      out.cmd(cmdBis, bl, al); out.cmd(cmdBis, bl, al); break;
                    case D | oSXor:
                    case D | oXor:     if(al.type==atReg) out.xor_r_a(al.reg, bl); else { out.cmd(cmdMov, al, Arg11::r4); out.xor_r_a(4, bl); }
                                       if(ah.type==atReg) out.xor_r_a(ah.reg, bh); else { out.cmd(cmdMov, ah, Arg11::r4); out.xor_r_a(4, bh); }
                                       break;
                    case D | oShl:     shift(al, ah, bl, true, pf); break;
                    case D | oShr:     shift(al, ah, bl, false, pf); break;
                    case D | oSet:
                    case D | oSetVoid: out.cmd(cmdMov, bl, al); out.cmd(cmdMov, bh, ah); break;
                    default: throw std::runtime_error("compile ntOperator cpu");
                }

                if(pop)
                {
                    popAcc_A2D(pf); //! Просто поменять A,D местами!!!
                }
                return;
            }

            if(r->o==oDiv || r->o==oMod || r->o==oMul || r->o==oShl || r->o==oShr || r->o==oSDiv || r->o==oSMod || r->o==oSMul || r->o==oSShl || r->o==oSShr)
            {
                Operator o = r->o;
                bool s = false;
                switch((unsigned)o)
                {
                    case oSDiv: o = oDiv; s = 1; break;
                    case oSMod: o = oMod; s = 1; break;
                    case oSMul: o = oMul; s = 1; break;
                    case oSShl: o = oShl; s = 1; break;
                    case oSShr: o = oShr; s = 1; break;
                }
                bool sf1 = r->dataType.isSigned();
                if(d==1) pushAcc(pf);
                compileVar(r->a, 0); //! Можно не вычислять два раза для S
                compileVar(r->b, 1);
                switch(o | pp)
                {
                    case oDiv | B:  out.call(sf1 ? "DIVBI" : "DIVBU"); break;
                    case oMod | B:  out.call(sf1 ? "MODBI" : "MODBU"); break;
                    case oMul | B:  out.call(sf1 ? "MULBI" : "MULBU"); break;
                    case oShl | B:  out.call("SHLB"); break;
                    case oShr | B:  out.call("SHRB"); break;
                    case oDiv | W:  out.call(sf1 ? "DIVWI" : "DIVWU"); break;
                    case oMod | W:  out.call(sf1 ? "MODWI" : "MODWU"); break;
                    case oMul | W:  out.call(sf1 ? "MULWI" : "MULWU"); break;
                    case oShl | W:  out.call("SHLW"); break;
                    case oShr | W:  out.call("SHRW"); break;
                    case oDiv | D:  out.call(sf1 ? "DIVDI" : "DIVDU"); break;
                    case oMod | D:  out.call(sf1 ? "MODDI" : "MODDU"); break;
                    case oMul | D:  out.call(sf1 ? "MULDI" : "MULDU"); break;
                    case oShl | D:  out.call("SHLD"); break;
                    case oShr | D:  out.call("SHRD"); break;
                    default: throw std::runtime_error("int1");
                }
                if(s)
                {
                    Arg11 al, ah;
                    compileArg(false, al, ah, r->a, 1, 1, pf); //! Можно не вычислять два раза для S
                    if(pp == B) out.cmd(cmdMovb, Arg11::r0, al); else
                    if(pp == W) out.cmd(cmdMov, Arg11::r0, al); else
                    if(pp == D) { out.cmd(cmdMov, Arg11::r0, al);  out.cmd(cmdMov, Arg11::r2, ah); } else throw std::runtime_error("!stack");
                }
                if(d==1) popAcc_A2D(pf);
                return;
            }

            if(r->o==oE || r->o==oNE || r->o==oL || r->o==oG || r->o==oLE || r->o==oGE)
            {
                if(pf=='D') throw std::runtime_error("compare 32");
                Arg11 al, ah, bl, bh;

                bool pop = compileDualArg(al, ah, bl, bh, true, d, r, pf);
                if(pf=='B') out.cmd(cmdCmpb, bl, al);
                       else out.cmd(cmdCmp, bl, al);
                if(pop) popAcc(pf); //!!!! Портит флаги. //!!!! Вместо регистра R0 или R2 использовать (SP)+

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
                    case oE:  out.cmd(cmdBeq,l); break;
                    case oNE: out.cmd(cmdBne,l); break;
                    case oL:  if(!r->a->dataType.isSigned()) out.cmd(cmdBhi, l); else out.cmd(cmdBgt,l); break;
                    case oG:  if(!r->a->dataType.isSigned()) out.cmd(cmdBlo, l); else out.cmd(cmdBlt,l); break;
                    case oLE: if(!r->a->dataType.isSigned()) out.cmd(cmdBhis,l); else out.cmd(cmdBge,l); break;
                    case oGE: if(!r->a->dataType.isSigned()) out.cmd(cmdBlos,l); else out.cmd(cmdBle,l); break;
                }

                if(ifOpt) return;

                unsigned nextLabel = out.labelsCnt++;
                out.cmd(cmdMov, Arg11(atValue,0,1), Arg11(atReg, d));
                out.cmd(cmdBr,nextLabel);
                out.addLocalLabel(l);
                out.cmd(cmdClr, Arg11(atReg, d));
                out.addLocalLabel(nextLabel);

                if(pop) popAcc(pf);
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
                out.cmd(cmdBr, j->label->n1);
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
            out.cmd(cmdBr, exitLabel);
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
                out.cmd(cmdBr, exitLabel);
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
// Компиляция команд сдвигов

bool CompilerPdp11::can_shift(NodeOperator* r)
{
    if(r->b->isNotConstI()) return false;
    unsigned v = r->b->cast<NodeConst>()->value;
    if(v == 0) return true; // Сдвиг не требуется
    if(r->dataType.is8()) return true;
    if(r->dataType.is16()) return true;
    if(r->dataType.is32() && (v==1 || v==8 || v==16 || v==24)) return true;
    return false;
}

void CompilerPdp11::shift(const Arg11& al, const Arg11& ah, const Arg11& bl, bool l, char pf)
{    
    //! Использовать временный регистр, если исрользуется несколько операций и AL-не регистр
    if(bl.type != atValue || !bl.str.empty()) throw std::runtime_error("compiler shift8");
    unsigned v = bl.value;
    if(v == 0) return;

    if(pf=='B')
    {
        if(l)
        {
            if(v <= 5) { for(unsigned i=0; i<v; i++) out.cmd(cmdAslb, al); return; }
            if(v <= 7) { for(unsigned i=0; i<9-v; i++) out.cmd(cmdRorb, al); out.cmd(cmdBicb, Arg11(atValue, 0, ~(0xFF << v)), al); return; }
        }
        else
        {
            if(v == 1) { out.clc(); out.cmd(cmdRorb, al); return; }
            if(v <= 4) { for(unsigned i=0; i<v; i++) out.cmd(cmdRorb, al); out.cmd(cmdBicb, Arg11(atValue, 0, ~(0xFF >> v)), al); return; }
            if(v <= 7) { for(unsigned i=0; i<9-v; i++) out.cmd(cmdRolb, al); out.cmd(cmdBicb, Arg11(atValue, 0, ~(0xFF >> v)), al); return; }
        }
        out.cmd(cmdClr, al); return;
        return;
    }

    if(pf=='W')
    {
        //! Может быть двигать в другую сторону. Например RORB+RORB+AND аналогично сдвигу на 7
        if(l)
        {
            if(v <= 7) { for(unsigned i=0; i<v; i++) out.cmd(cmdAsl, al); return; }
            if(v <= 15) { out.cmd(cmdSwab, al); out.cmd(cmdClrb, al); for(unsigned i=8; i<v; i++) out.cmd(cmdAsl, al); return; }
        }
        else
        {
            if(v == 1) { out.clc(); out.cmd(cmdRor, al); return; }
            if(v <= 7) { for(unsigned i=0; i<v; i++) out.cmd(cmdRor, al); out.cmd(cmdBic, Arg11(atValue, 0, ~(0xFFFF >> v)), al); return; }
            if(v == 8) { out.cmd(cmdClrb, al); out.cmd(cmdSwab, al); return; }
            if(v <= 15) { out.cmd(cmdSwab, al); for(unsigned i=8; i<v; i++) out.cmd(cmdRor, al); out.cmd(cmdBic, Arg11(atValue, 0, ~(0xFFFF >> v)), al); return; }
        }
        out.cmd(cmdClr, al);
        return;
    }

    if(pf=='D')
    {
        //! Может быть двигать в другую сторону. Например RORB+RORB+AND аналогично сдвигу на 7
        if(l)
        {
            if(v <= 15) { for(unsigned i=0; i<v; i++) { out.cmd(cmdAsl, al); out.cmd(cmdRol, ah); } return; }
            if(v <= 23) { out.cmd(cmdMov, al, ah); out.cmd(cmdClr, al); for(unsigned i=16; i<v; i++) out.cmd(cmdAsl, ah); return; }
            if(v <= 31) { out.cmd(cmdMov, al, ah); out.cmd(cmdClr, al); out.cmd(cmdSwab, ah); out.cmd(cmdClrb, ah); for(unsigned i=24; i<v; i++) out.cmd(cmdAsl, ah); return; }
        }
        else
        {
            uint32_t m = 0xFFFFFFFF >> v;
            if(v == 1) { out.clc(); out.cmd(cmdRor, ah); out.cmd(cmdRor, al); return; }
            if(v <= 15) { for(unsigned i=8; i<v; i++) { out.cmd(cmdRor, ah); out.cmd(cmdRor, al); } out.cmd(cmdBic, Arg11(atValue, 0, ~(m>>16)), ah); return; }
            if(v == 16) { out.cmd(cmdMov, ah, al); out.cmd(cmdClr, ah); return; }
            if(v <= 23) { out.cmd(cmdMov, ah, al); out.cmd(cmdClr, ah); for(unsigned i=16; i<v; i++) out.cmd(cmdRor, al); out.cmd(cmdBic, Arg11(atValue, 0, ~m), al); return; }
            if(v <= 31) { out.cmd(cmdMov, ah, al); out.cmd(cmdClr, ah); out.cmd(cmdSwab, ah); for(unsigned i=24; i<v; i++) out.cmd(cmdRor, al); out.cmd(cmdBic, Arg11(atValue, 0, ~m), al); return; }
        }
        out.cmd(cmdClr, al);
        out.cmd(cmdClr, ah);
        return;
    }

    throw std::runtime_error("CompilerPdp11::shift");
}

//---------------------------------------------------------------------------------------------------------------------

void CompilerPdp11::compileBlock(Node* n)
{
    for(; n; n=n->next)
        compileVar(n, 0);
}

//---------------------------------------------------------------------------------------------------------------------

void CompilerPdp11::compileFunction(Function* f)
{
    if(f->compiled || !f->parsed) return;

    out.c.lstWriter.remark(out.c.out.writePtr, 1, f->name);
    compiler.addLabel(f->name);

    curFn = f;
    if(f->stackSize) out.cmd(cmdSub, Arg11(atValue, 0, f->stackSize), Arg11::sp); //! Использовать PUSH

    //! Вместо двух проходов сделать промежуточный ассемблер

    treePrepare(f);

    out.step0();
    out.labelsCnt = f->labelsCnt;
    returnLabel = out.labelsCnt++;
    compileBlock(f->root);
    out.addLocalLabel(returnLabel);
    // Тут может уже быть RET или JMP сюда же
    if(f->stackSize) out.cmd(cmdAdd, Arg11(atValue, 0, f->stackSize), Arg11::sp); //! Использовать POP
    out.ret();

    out.step1();
    out.labelsCnt = f->labelsCnt;
    returnLabel = out.labelsCnt++;
    compileBlock(f->root);
    out.addLocalLabel(returnLabel);
    if(f->stackSize) out.cmd(cmdAdd, Arg11(atValue, 0, f->stackSize), Arg11::sp); //! Использовать POP
    out.ret();

    out.step2();

    curFn = 0;
    f->compiled = true;
}

//---------------------------------------------------------------------------------------------------------------------

void CompilerPdp11::start(unsigned step)
{
    inStack = 0;
    if(step == 0)
    {
        compiler.out.align2();

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
        if(writePtrs.empty()) throw std::runtime_error("CompilerPdp11.compile");
        unsigned s = compiler.out.writePtr, e = writePtrs.front();
        writePtrs.pop();

        compiler.disassembly(s,e);

        if(writePtrs.empty()) throw std::runtime_error("CompilerPdp11.compile");
        compiler.out.writePtr = writePtrs.front();
        writePtrs.pop();
    }
}

//---------------------------------------------------------------------------------------------------------------------

}
