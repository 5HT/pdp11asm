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
    out.push(Arg11::r0);
    if(pf=='D') out.push(Arg11::r2);
}

//---------------------------------------------------------------------------------------------------------------------

void CompilerPdp11::popAcc(char pf)
{
    if(pf=='D') out.pop(Arg11::r2);
    out.pop(Arg11::r0);
}

//---------------------------------------------------------------------------------------------------------------------

void CompilerPdp11::popAcc_A2D(char pf)
{
    out.cmd(cmdMov, Arg11::r0, Arg11::r1);
    out.pop(Arg11::r0);
    if(pf=='D')
    {
        out.cmd(cmdMov, Arg11::r2, Arg11::r3);
        out.pop(Arg11::r2);
    }
}

//---------------------------------------------------------------------------------------------------------------------

bool CompilerPdp11::compileArg(Arg11& ol, Arg11& oh, NodeVar* a, int d, int dr, char pf)
{
    while(a->nodeType == ntConvert && a->dataType.addr!=0 && ((NodeConvert*)a)->var->dataType.addr!=0)
        a = ((NodeConvert*)a)->var;

    // SP можно разметить в аргументе
    if(a->nodeType == ntSP)
    {
        ol.type = atReg;
        ol.reg = 6;
        oh.type = atValue;
        oh.value = 0;
        return false;
    }

    // Число можно разместить в аргументе
    if(a->nodeType == ntConstI)
    {
        unsigned v = a->cast<NodeConst>()->value;
        ol.type = atValue;
        ol.value = (v & 0xFFFF);
        oh.type = atValue;
        oh.value = (v >> 16);
        return false;
    }
    if(a->nodeType == ntConstS)
    {
        ol.type = atValue;
        ol.str = a->cast<NodeConst>()->text;
        oh.type = atValue;
        oh.value = 0;
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
        if(b->nodeType == ntConstI)
        {
            ol.type = atValueMem;
            ol.value = b->cast<NodeConst>()->value;
            oh.type = atValueMem;
            oh.value = b->cast<NodeConst>()->value + 2;
            return false;
        }
        if(b->nodeType == ntConstS)
        {
            ol.type = atValueMem;
            ol.str = b->cast<NodeConst>()->text;
            oh.type = atValueMem;
            oh.str = b->cast<NodeConst>()->text + "+2";
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
                if(e->a->nodeType != ntConstI && e->b->nodeType == ntConstI) std::swap(e->a, e->b);
                if(e->a->nodeType == ntConstI)
                {
                    int off = e->a->cast<NodeConst>()->value;
                    if(e->b->nodeType == ntSP) // Оптимизация Deaddr(Add(Const,SP)) одной командой
                    {
                        ol.type = dbl ? atRegValueMemMem : atRegValueMem;
                        ol.reg  = 6;
                        ol.value = off;
                        oh.type = ol.type;
                        oh.reg  = 6;
                        oh.value = off+2;
                        return false;
                    }
                    // Оптимизация Deaddr(Add(Const,REG)) одной командой
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
            ol.value = 0;
            oh.type = ol.type;
            oh.value = 2;
            oh.reg = 6;
            return false;
        }

        //! Двойной адрес

        // Оптимизация Deaddr(REG) одной командой
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

void CompilerPdp11::compileVar(Node* n, unsigned d, IfOpt* ifOpt)
{
//    printf("---%u\n", n->nodeType);

    if(!n->remark.empty())
        out.c.lstWriter.remark(out.c.out.writePtr, 0, n->remark.c_str());

    switch(n->nodeType)
    {
        case ntLabel: // Метка
            out.addLocalLabel(n->cast<NodeLabel>()->n);
            return;

        case ntSP: // Поместить SP в аккумулятор
            out.cmd(cmdMov, Arg11::sp, Arg11(atReg, d));
            return;

        case ntConstI: // Числовая константа
        {
            NodeConst* c = (NodeConst*)n;
            out.cmd(cmdMov, Arg11(atValue, c->value % 0xFFFF), Arg11(atReg, d));
            if(c->dataType.is32()) out.cmd(cmdMov, Arg11(atValue, c->value >> 16), Arg11(atReg, d+2));
            return;
        }

        case ntConstS: // Константа
            out.cmd(cmdMov, Arg11(atValue, 0), Arg11(atReg, d));
            out.c.addFixup(n->cast<NodeConst>()->text.c_str(), 2);
            return;

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
                        case cbtUChar:  out.cmd(cmdBic, Arg11(atValue, 0xFF00), Arg11(atReg, d)); return;
                    }
                    return;
                case cbtLong:
                case cbtULong:
                    switch((unsigned)f)
                    {
                        case cbtChar:   out.call(d ? "SGNB1" : "SGNB0"); out.call(d ? "SGNW1" : "SGNW0"); return;
                        case cbtUChar:  out.cmd(cmdBic, Arg11(atValue, 0xFF00), Arg11(atReg, d)); out.cmd(cmdClr, Arg11(atReg, d+2)); return;
                        case cbtShort:  out.call(d ? "SGNW1" : "SGNW0"); return;
                        case cbtUShort: out.cmd(cmdClr, Arg11(atReg, d+2)); return;
                    }
            }
            return;
        }

        case ntCallI: // Вызов функции
        case ntCallS:
        {
            NodeCall* c = n->cast<NodeCall>();

            if(d==1) pushAcc('D');

            unsigned ss = 0;
            for(int i=c->args.size()-1; i>=0; i--)
            {
                NodeVar* v = c->args[i];
                //dumpTree1(v, 0);

                char pf;
                if(v->dataType.is8()) pf='B'; else
                if(v->dataType.is16()) pf='W'; else
                if(v->dataType.is32()) pf='D'; else throw std::runtime_error("monoOperator !stack");

                Arg11 al,ah;
                compileArg(al,ah,v,0,0,pf);

                if(v->dataType.is8_16()) { ss+=2; out.push(al); } else //! Может в PUSH затолкнуть?
                if(v->dataType.is32()) { ss+=4; out.push(ah); out.push(al); } else throw std::runtime_error("call !stack");
            }

            //! args?
            if(c->nodeType == ntCallS) out.call(c->name.c_str());
                                  else out.call(c->addr);

            out.cmd(cmdAdd, Arg11(atValue, 0, ss), Arg11::sp);

            if(d==1)
            {
                out.cmd(cmdMov, Arg11::r0, Arg11::r1);
                if(c->dataType.is32()) out.cmd(cmdMov, Arg11::r2, Arg11::r3);
                popAcc('D');
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
            compileArg(al, ah, c, d, d, pf);

            // Результат уже в нужном регистре
            if(al.type != atReg || al.reg != d) out.cmd(pf=='B' ? cmdMovb : cmdMov, al, Arg11(atReg, d));
            if(pf == 'D') if(ah.type != atReg || ah.reg != d+2) out.cmd(cmdMov, ah, Arg11(atReg, d+2));

            return;
        }

        case ntSwitch: // Оптимизированый выбор
            throw std::runtime_error("switch");

        case ntReturn:
        {
            NodeReturn* r = n->cast<NodeReturn>();
            if(d != 0) throw std::runtime_error("return !d");
            if(r->var) compileVar(r->var, 0);
            out.cmd(cmdAdd, Arg11(atValue, 0, curFn->stackSize), Arg11::sp);
            out.ret();
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
                compileArg(al, ah, o->a, d, d, pf);
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
                compileArg(al, ah, o->a, d, d, pf);
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
                    unsigned nextLabel = tree.intLabels++;
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

                unsigned trueLabel = tree.intLabels++;
                unsigned falseLabel = tree.intLabels++;
                unsigned nextLabel = tree.intLabels++;

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

            if(r->o==oDiv || r->o==oMod || r->o==oMul || r->o==oShl || r->o==oShr)
            {
                char sf = r->dataType.isSigned() ? 'I' : 'U';
                if(d==1) pushAcc(pf);
                compileVar(r->a, 0);
                compileVar(r->b, 1);
                switch(r->o | pp)
                {
                    case oDiv | B:  out.call(sf ? "DIVBI" : "DIVBU"); break;
                    case oMod | B:  out.call(sf ? "MODBI" : "MODBU"); break;
                    case oMul | B:  out.call(sf ? "MULBI" : "MULBU"); break;
                    case oShl | B:  out.call("SHLB"); break;
                    case oShr | B:  out.call("SHRB"); break;
                    case oDiv | W:  out.call(sf ? "DIVWI" : "DIVWU"); break;
                    case oMod | W:  out.call(sf ? "MODWI" : "MODWU"); break;
                    case oMul | W:  out.call(sf ? "MULWI" : "MULWU"); break;
                    case oShl | W:  out.call("SHLW"); break;
                    case oShr | W:  out.call("SHRW"); break;
                    case oDiv | D:  out.call(sf ? "DIVDI" : "DIVDU"); break;
                    case oMod | D:  out.call(sf ? "MODDI" : "MODDU"); break;
                    case oMul | D:  out.call(sf ? "MULDI" : "MULDU"); break;
                    case oShl | D:  out.call("SHLD"); break;
                    case oShr | D:  out.call("SHRD"); break;
                    default: throw std::runtime_error("int1");
                }

                if(d==1) popAcc_A2D(pf);
                return;
            }

            if(r->o==oAdd || r->o==oSub || r->o==oAnd || r->o==oOr || r->o==oXor || r->o==oSetVoid || r->o==oSet)
            {
                Arg11 al, ah, bl, bh;
                bool pop = false;
                if(r->o==oSetVoid || r->o==oSet)
                {
                    pop = compileArg(al, ah, r->a, 0, d, pf); //! для oSet аккумулятор долежн содержать устанаввливаемое значение
                }
                else
                {
                    if(d==1)
                    {
                        pushAcc(pf);
                        pop = true;
                    }
                    compileVar(r->a, 0);
                    al.type = atReg;
                    al.reg = 0;
                    ah.type = atReg;
                    ah.reg = 2;
                }
                compileArg(bl, bh, r->b, d==0 && !al.regUsed() ? 0 : 1, d, pf); // Если R0 не занят, занять его!

                switch(r->o | pp)
                {
                    case B | oAdd:     throw std::runtime_error("ADD8!");
                    case B | oSub:     throw std::runtime_error("SUB8!");
                    case B | oAnd:     out.cmd(cmdBicb, bl, al); break; //!!! AND нет
                    case B | oOr:      out.cmd(cmdBisb, bl, al); break;
                    case B | oXor:     if(bl.type==atReg) out.xor_r_a(bl.reg, al); else { out.cmd(cmdMov, bl, Arg11::r4); out.xor_r_a(4, al); } break; //!!! нет 8 бит
                    case B | oSet:
                    case B | oSetVoid: if(bl.type == atValue && bl.value == 0) out.cmd(cmdClrb, al); else out.cmd(cmdMovb, bl, al); break;
                    case W | oAdd:     out.cmd(cmdAdd, bl, al); break;
                    case W | oSub:     out.cmd(cmdSub, bl, al); break;
                    case W | oAnd:     out.cmd(cmdBic, bl, al); break; //!!! AND нет
                    case W | oOr:      out.cmd(cmdBis, bl, al); break;
                    case W | oXor:     if(bl.type==atReg) out.xor_r_a(bl.reg, al); else { out.cmd(cmdMov, bl, Arg11::r4); out.xor_r_a(4, al); } break;
                    case W | oSet:
                    case W | oSetVoid: if(bl.type == atValue && bl.value == 0) out.cmd(cmdClr, al); else out.cmd(cmdMov, bl, al); break;
                    case D | oAdd:     out.cmd(cmdAdd, bl, al); out.cmd(cmdAdc, ah); out.cmd(cmdAdd, bh, ah); break;
                    case D | oSub:     out.cmd(cmdSub, bl, al); out.cmd(cmdSbc, ah); out.cmd(cmdSub, bh, ah); break;
                    case D | oAnd:     out.cmd(cmdBic, bl, al); out.cmd(cmdBic, bl, al); break; //!!! AND нет
                    case D | oOr:      out.cmd(cmdBis, bl, al); out.cmd(cmdBis, bl, al); break;
                    case D | oXor:     if(al.type==atReg) out.xor_r_a(al.reg, bl); else { out.cmd(cmdMov, al, Arg11::r4); out.xor_r_a(4, bl); }
                                       if(ah.type==atReg) out.xor_r_a(ah.reg, bh); else { out.cmd(cmdMov, ah, Arg11::r4); out.xor_r_a(4, bh); }
                                       break;
                    case D | oSet:
                    case D | oSetVoid: if(bl.type == atValue && bl.value == 0) out.cmd(cmdClr, al); else out.cmd(cmdMov, bl, al);
                                       if(bh.type == atValue && bh.value == 0) out.cmd(cmdClr, ah); else out.cmd(cmdMov, bh, ah);
                                       break;
                }

                if(pop)
                {
                    popAcc_A2D(pf); //! Просто поменять A,D местами!!!
                }
                return;
            }

            if(r->o==oE || r->o==oNE || r->o==oL || r->o==oG || r->o==oLE || r->o==oGE)
            {
                if(pf=='D') throw std::runtime_error("compare 32");
                Arg11 al, ah, bl, bh;
                bool pop = compileArg(al, ah, r->a, 0, d, pf);
                pop |= compileArg(bl, bh, r->b, d==0 && !al.regUsed() ? 0 : 1, d, pf);

                if(pf=='B') out.cmd(cmdCmpb, bl, al);
                       else out.cmd(cmdCmp, bl, al);
                if(pop) popAcc(pf);

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
                    l = tree.intLabels++;
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

                unsigned nextLabel = tree.intLabels++;
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
            NodeJmp* j = (NodeJmp*)n;
            if(!j->cond)
            {
                out.cmd(cmdBr, j->label->n);
                return;
            }
            compileJump(j->cond, 0, !j->ifZero, j->label->n);
            return;
        }

        case ntIf:
        {
            NodeIf* r = (NodeIf*)n;

            unsigned falseLabel = tree.intLabels++;
            compileJump(r->cond, 0, false, falseLabel);
            compileBlock(r->t);
            if(r->f == 0)
            {
                out.addLocalLabel(falseLabel);
            }
            else
            {
                unsigned exitLabel = tree.intLabels++;
                out.cmd(cmdBr,exitLabel);
                out.addLocalLabel(falseLabel);
                compileBlock(r->f);
                out.addLocalLabel(exitLabel);
            }
            return;
        }

        default:
            throw std::runtime_error("Unknown node");
    }
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
    if(f->compiled || !f->rootNode) return;

    out.c.lstWriter.remark(out.c.out.writePtr, 1, f->name);

    compiler.labels[ucase(f->name)] = compiler.out.writePtr;

    curFn = f;
    out.resetLocalLabels();
    if(f->stackSize) out.cmd(cmdSub, Arg11(atValue, 0, f->stackSize), Arg11::sp); //! Использовать PUSH
    compileBlock(f->rootNode);
    out.processLocalLabels();
    curFn = 0;
    f->compiled = true;
}

//---------------------------------------------------------------------------------------------------------------------

void CompilerPdp11::start(unsigned step)
{
    if(step == 0)
    {
        for(std::list<C::Function>::iterator i=tree.functions.begin(); i!=tree.functions.end(); i++) // МОжно начинать не с начала
            compileFunction(&*i);
        writePtrs.push(this->out.c.out.writePtr);
    }
    else
    {
        if(writePtrs.empty()) throw std::runtime_error("CompilerPdp11.compile");
        unsigned s = compiler.out.writePtr, e = writePtrs.front();
        writePtrs.pop();

        compiler.disassembly(s,e);
    }
}

//---------------------------------------------------------------------------------------------------------------------

}
