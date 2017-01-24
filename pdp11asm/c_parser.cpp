// PDP11 Assembler (c) 08-01-2017 ALeksey Morozov (aleksey.f.morozov@gmail.com)

#include "c_parser.h"
#include "parser.h"
#include <string>
#include <map>
#include <stdio.h>
#include <string.h>
#include "tools.h"

namespace C
{

//---------------------------------------------------------------------------------------------------------------------

Parser::StackVar* Parser::ifToken(std::vector<StackVar>& a)
{
    for(std::vector<StackVar>::iterator i=a.begin(); i!=a.end(); i++)
        if(p.ifToken(i->name.c_str()))
            return &*i;
    return 0;
}

//---------------------------------------------------------------------------------------------------------------------

Function* Parser::ifToken(std::list<Function>& a)
{
    for(std::list<Function>::iterator f = a.begin(); f!=a.end(); f++)
        if(p.ifToken(f->name.c_str()))
            return &*f;
    return 0;
}

//---------------------------------------------------------------------------------------------------------------------

GlobalVar* Parser::ifToken(std::list<GlobalVar>& a)
{
    std::list<GlobalVar>::iterator f = a.begin();
    for(; f!=a.end(); f++)
        if(p.ifToken(f->name.c_str()))
            return &*f;
    return 0;
}

//---------------------------------------------------------------------------------------------------------------------

bool Parser::checkStackUnique(const char* str) //! Заменить это потом на более оптимальное
{
    for(std::vector<StackVar>::iterator i=stackVars.begin(); i!=stackVars.end(); i++)
        if(i->name == str)
            return false;
    return true;
}

//---------------------------------------------------------------------------------------------------------------------

NodeVar* Parser::getStackVar(StackVar& x)
{
    Type t = x.type;
    if(!t.arr) t.addr++;
    NodeConst* c = outOfMemory(new NodeConst(x.addr, t)); // curFn->name+"_"+x.name
    if(x.arg) c->prepare = true;
    NodeVar* r = outOfMemory(new NodeOperator(c->dataType, oAdd, c, outOfMemory(new NodeSP)));
    if(!t.arr)
    {
        if(r->dataType.addr==0) p.syntaxError("getStackVar NodeDeaddr");
        r = outOfMemory(new NodeDeaddr(r));
    }
    return r;
}

//---------------------------------------------------------------------------------------------------------------------

NodeVar* Parser::nodeConvert(NodeVar* x, Type type, bool auto_convert)
{
    // Преобразовывать не надо
    if(type.baseType == x->dataType.baseType && type.addr == x->dataType.addr) return x;

    // Константы преобразуются налету
    if(x->isConst())
    {
        if(x->nodeType==ntConstI)
        {
            if(type.is8()) x->cast<NodeConst>()->value &= 0xFF; else //! Предупреждение о переполнении
            if(type.is16()) x->cast<NodeConst>()->value &= 0xFFFF;
        }
        x->dataType = type;
        return x;
    }

    // 8 битные арифметические операции
/*    if(x->nodeType==ntOperator && type.is8())
    {
        NodeOperator* no = x->cast<NodeOperator>();
        if((no->a->nodeType==ntConvert || no->a->isConst()) && (no->b->nodeType==ntConvert || no->b->isConst()))
        {
            if(no->o==oAdd || no->o==oSub || no->o==oMul || no->o==oAnd || no->o==oOr || no->o==oXor)
            { //! Добавить остальные
                // Обрезаем конвертации
                if(no->a->nodeType==ntConvert) no->a->cast<NodeConvert>()->dataType = type; else no->a->dataType = type;
                if(no->b->nodeType==ntConvert) no->b->cast<NodeConvert>()->dataType = type; else no->b->dataType = type;
                no->dataType = type;
                return no;
            }
        }
    }*/

    // Преобразовывать надо
    return new NodeConvert(x, type);
}

//---------------------------------------------------------------------------------------------------------------------

NodeVar* Parser::bindVar_2()
{
    if(p.ifToken(ttString2)) //! Не обрабатываются строки с \x00 !!!
    {
        std::string buf;
        buf += std::string(p.loadedText, p.loadedTextSize);
        while(p.ifToken(ttString2))
            buf += std::string(p.loadedText, p.loadedTextSize);
        Type type;
        type.addr = 1;
        type.baseType = cbtChar;
        return outOfMemory(new NodeConst(world.regString(buf.c_str()), type));
    }    
    if(p.ifToken(ttString1)) {
        return outOfMemory(new NodeConst((unsigned char)p.loadedText[0], cbtChar));
    }
    if(p.ifToken(ttInteger)) {        
        return outOfMemory(new NodeConst(p.loadedNum, p.loadedNum < 256 ? cbtUChar : p.loadedNum < 65536 ? cbtUShort : cbtULong)); //! Определить ти переменной
    }
    if(p.ifToken("sizeof")) {
        p.needToken("(");
        // Если там указан тип, то нет проблем
        Type type1 = readType(false);
        if(type1.baseType == cbtError) {
            NodeVar* a = readVar(-1);
            type1 = a->dataType;
            delete a;
        }
        p.needToken(")");
        return new NodeConst(type1.size(), cbtShort);
    }

    // Это либо просто скобки, либо преобразование типов
    if(p.ifToken("(")) {
        // Преобразование типа
        Type type = readType(false);
        if(type.baseType != cbtError) {
            readModifiers(type);
            p.needToken(")");
            return nodeConvert(bindVar(), type, false);
        }
        NodeVar* a = readVar(-1);
        p.needToken(")");
        return a;
    }

    // Стековая переменная
    StackVar* s = ifToken(stackVars);
    if(s) return getStackVar(*s);

    // Глобальная переменная
    GlobalVar* g = ifToken(world.globalVars);
    if(g)
    {
        NodeConst* c = new NodeConst(g->name, g->type);
        if(g->type.arr) return c;
        c->dataType.addr++;
        return new NodeDeaddr(c);
    }

    // Это функция
    Function* f;
    if((f = ifToken(world.functions)) != 0)
    {
        p.needToken("(");

        // Чтение аргументов
        NodeCall* c =  new NodeCall(f->addr, f->name, f->retType, f);
        outOfMemory(c);
        std::vector<NodeVar*> args;
        bool fi = true;
        for(std::vector<FunctionArg>::iterator jj=f->args.begin(); jj!=f->args.end(); jj++) {
            if(p.ifToken(")")) p.syntaxError("Функция требует больше параметров");
            if(fi) fi=false; else p.needToken(",");
            c->args1.push_back(nodeConvert(readVar(), jj->type, true));
        }
        if(p.ifToken(",")) p.syntaxError("Функция требует меньше параметров");
        p.needToken(")");

        return c;
    }

    p.syntaxError();
    return 0;
}

//---------------------------------------------------------------------------------------------------------------------

//! Заменить ((SP+const)+const)

NodeVar* Parser::nodeOperator2(Type type, Operator o, NodeVar* a, NodeVar* b)
{
    // Операторы сравнения всегда дают short
    if(o==oE || o==oNE || o==oG || o==oGE || o==oL || o==oLE || o==oLAnd || o==oLOr) type = cbtShort;

    // Операции между константами
    if(a->nodeType==ntConstI && b->nodeType==ntConstI)
    {
        uint32_t ac = a->cast<NodeConst>()->value;
        uint32_t bc = b->cast<NodeConst>()->value;
        delete a;
        delete b;
        switch(o)
        {
            case oAdd: ac = (ac +  bc); break;
            case oSub: ac = (ac -  bc); break;
            case oAnd: ac = (ac &  bc); break;
            case oOr:  ac = (ac |  bc); break;
            case oXor: ac = (ac ^  bc); break;
            case oMul: if(type.isSigned()) ac = int32_t(ac) *  int32_t(bc); else ac = (ac *  bc); break;
            case oDiv: if(type.isSigned()) ac = int32_t(ac) /  int32_t(bc); else ac = (ac /  bc); break;
            case oShr: ac = (ac >> bc); break;
            case oShl: ac = (ac << bc); break;
            case oNE:  ac = (ac != bc ? 1 : 0); break;
            case oE:   ac = (ac == bc ? 1 : 0); break;
            case oGE:  ac = (ac >= bc ? 1 : 0); break;
            case oG:   ac = (ac >  bc ? 1 : 0); break;
            case oLE:  ac = (ac <= bc ? 1 : 0); break;
            case oL:   ac = (ac <  bc ? 1 : 0); break;
            default: throw std::runtime_error("nodeOperator2");
        }
        return outOfMemory(new NodeConst(ac, type));
    }

    // Умножение на единицу бессмысленно
    if(o==oMul)
    {
        if(a->nodeType==ntConstI && a->cast<NodeConst>()->value==1) return b;
        if(b->nodeType==ntConstI && b->cast<NodeConst>()->value==1) return a;
    }

    // Деление тоже
    if(o==oDiv)
    {
        if(b->nodeType==ntConstI && b->cast<NodeConst>()->value==1) return a;
    }

    // Фича PDP
    if(o == oAnd || o==oSAnd) b = nodeMonoOperator(b, moXor); //!!! Приоритет у константы или регистра (т.е. прошлое вычисление дало регистр)

    return outOfMemory(new NodeOperator(type, o, a, b));
}

//---------------------------------------------------------------------------------------------------------------------

NodeVar* Parser::nodeOperator(Operator o, NodeVar* a, NodeVar* b, bool noMul, NodeVar* cond)
{
    // Этим операторам типы не важны
    if(o==oLAnd || o==oLOr) return nodeOperator2(cbtShort, o, a, b);

    // Сложение указателя и числа
    if(o==oAdd && a->dataType.addr!=0 && b->dataType.addr==0 && b->dataType.is8_16()) //! void*
    {
        b = nodeConvert(b, cbtUShort, true);
        if(!noMul) { unsigned s = a->dataType.sizeElement(); if(s != 1) b = nodeOperator(oMul, b, new NodeConst(s, cbtUShort)); }
        return nodeOperator2(a->dataType, o, a, b);
    }

    // Сложение числа и указателя
    if(o==oAdd && a->dataType.addr==0 && a->dataType.is8_16() && b->dataType.addr!=0) //! void*
    {
        a = nodeConvert(a, cbtUShort, true);
        if(!noMul) { unsigned s = b->dataType.sizeElement(); if(s != 1) a = nodeOperator(oMul, a, new NodeConst(s, cbtUShort)); }
        return nodeOperator2(b->dataType, o, a, b);
    }

    // Вычитание числа из указателя
    if(o==oSub && a->dataType.addr!=0 && b->dataType.addr==0 && b->dataType.is8_16()) //! void*
    {
        b = nodeConvert(b, cbtUShort, true);
        if(!noMul) { unsigned s = a->dataType.sizeElement(); if(s != 1) b = nodeOperator(oMul, b, new NodeConst(s, cbtUShort)); }
        return nodeOperator2(a->dataType, o, a, b);
    }

    // Операция между указателем и нулем
    if(a->dataType.addr!=0 && b->dataType.addr==0 && b->nodeType==ntConstI && b->cast<NodeConst>()->isNull())
        b = nodeConvert(b, a->dataType, false);

    // Операция между нулем и указателем
    if(b->dataType.addr!=0 && a->dataType.addr==0 && a->nodeType==ntConstI && a->cast<NodeConst>()->isNull())
        a = nodeConvert(a, b->dataType, false);

    // Вычитание указателя из указателя. И типы указателей идентичны
    if(o==oSub && a->dataType.addr!=0 && a->dataType.addr==b->dataType.addr && a->dataType.baseType==b->dataType.baseType)
    {
        unsigned s = a->dataType.sizeElement();
        if(s != 0) // Это void*
        {
            NodeVar* n = nodeOperator2(cbtUShort, o, a, b);
            if(!noMul) if(s != 1) n = nodeOperator(oDiv, n, new NodeConst(s, cbtUShort));
            return n;
        }
    }

    // Сравнение указателей
    if((o==oE || o==oNE || o==oL || o==oG || o==oLE || o==oGE) && a->dataType.addr!=0 && b->dataType.addr!=0)
        return nodeOperator2(cbtVoid, o, a, b);

    // Услованый оператор для указателей
    if(o==oIf && a->dataType.addr!=0 && b->dataType.addr!=0)
    {
        Type type = a->dataType;
        if(a->dataType.addr!=b->dataType.addr || a->dataType.baseType!=b->dataType.baseType) type.baseType=cbtVoid, type.addr=1;
        return world.allocOperatorIf(cond, nodeConvert(a, type, true), nodeConvert(b, type, true));
    }        

    // При записи всё просто. Приводим к типу приёмника.        
    if(o==oSet || o==oSetVoid || o==oSAdd || o==oSSub || o==oSMul || o==oSDiv
    || o==oSMod || o==oSShl || o==oSShr || o==oSAnd || o==oSXor || o==oSOr)
        return nodeOperator2(a->dataType, o, a, nodeConvert(b, a->dataType, true));

    // Результирующий тип
    unsigned r = 1;
    bool sig = false;
    operatorType(r, sig, a->dataType);
    operatorType(r, sig, b->dataType);
    // Арифметические операции между 8 битными числами дают 16 битный результат
    if(r==1 && (o==oAdd || o==oSub || o==oDiv || o==oMod || o==oMul)) r=2;
    Type resultType = r>=4 ? (sig ? cbtLong : cbtULong) : r>=2 ? (sig ? cbtShort : cbtUShort) : (r>=1 ? cbtChar : cbtUChar);


    // Условный оператор
    if(o==oIf) return world.allocOperatorIf(cond, nodeConvert(a, resultType, true), nodeConvert(b, resultType, true));
    return nodeOperator2(resultType, o, nodeConvert(a, resultType, true), nodeConvert(b, resultType, true));
}

//---------------------------------------------------------------------------------------------------------------------

void Parser::operatorType(unsigned& r, bool& sig, Type& a)
{
    if(a.addr) p.syntaxError("Такая операция между указателями невозможна");
    switch(a.baseType)
    {
        case cbtVoid:   p.syntaxError("Операции с типом void недопустимы");
        case cbtChar:   r |= 1; sig = true; break;
        case cbtUChar:  r |= 1; break;
        case cbtShort:  r |= 2; sig = true; break;
        case cbtUShort: r |= 2; break;
        case cbtLong:   r |= 4; sig = true; break;
        case cbtULong:  r |= 4; break;
        default: p.syntaxError("Такая операция между этими типами невозможна");
    }
}

//---------------------------------------------------------------------------------------------------------------------

NodeVar* Parser::nodeAddr(NodeVar* x)
{
    if(x->nodeType != ntDeaddr) p.syntaxError("nodeAddr");
    NodeDeaddr* a = x->cast<NodeDeaddr>();
    NodeVar* result = a->var;
    a->var = 0;
    delete a;
    return result;
}

//---------------------------------------------------------------------------------------------------------------------

NodeVar* Parser::nodeMonoOperator(NodeVar* a, MonoOperator o)
{    
    if(o == moDeaddr) return outOfMemory(new NodeDeaddr(a));
    if(o == moAddr) return nodeAddr(a);

    // Моно оператор над константой
    if(a->nodeType == ntConstI)
    {
        NodeConst* ac = a->cast<NodeConst>();
        switch((unsigned)o) {
            case moNeg: ac->value = -ac->value; return ac;
            case moXor: ac->value = ~ac->value; return ac;
            case moNot: ac->value = !ac->value; return ac;
            //! Вывести ошибку, что остальные операции запрещены
        }
    }

    if(o==moIncVoid || o==moDecVoid) throw std::runtime_error("moVoid");

    if(o==moPostInc || o==moPostDec)
    {
        if(a->nodeType != ntDeaddr) p.syntaxError("Требуется переменная");
    }

    if(o==moInc || o==moDec)
    {
        if(a->nodeType != ntDeaddr) p.syntaxError("Требуется переменная");
        NodeDeaddr* aa = a->cast<NodeDeaddr>();
        NodeVar* b = aa->var;
        aa->var = 0;
        delete aa;
        NodeVar* x = new NodeMonoOperator(b, o);
        x = new NodeDeaddr(x);
        return x;
    }

    return new NodeMonoOperator(a, o);
}

//---------------------------------------------------------------------------------------------------------------------

NodeVar* Parser::bindVar()
{
    // Чтение монооператора, выполнять будем потом
    std::vector<MonoOperator> mo;
    while(true) {
        if(p.ifToken("+" )) continue;
        if(p.ifToken("~" )) { mo.push_back(moXor   ); continue; }
        if(p.ifToken("*" )) { mo.push_back(moDeaddr); continue; }
        if(p.ifToken("&" )) { mo.push_back(moAddr  ); continue; }
        if(p.ifToken("!" )) { mo.push_back(moNot   ); continue; }
        if(p.ifToken("-" )) { mo.push_back(moNeg   ); continue; }
        if(p.ifToken("++")) { mo.push_back(moInc   ); continue; }
        if(p.ifToken("--")) { mo.push_back(moDec   ); continue; }
        break;
    }

    NodeVar* a = bindVar_2();

    for(;;)
    {
    retry:
        if(p.ifToken("->"))
        {
            a = new NodeDeaddr(a);
            goto xx;
        }
        if(p.ifToken("."))
        {
xx:         if(a->dataType.baseType!=cbtStruct || a->dataType.addr!=0 || a->dataType.s==0) p.syntaxError("Ожидается структура");
            Struct& s = *a->dataType.s;
            for(unsigned int i=0; i<s.items.size(); i++)
            {
                StructItem& si = s.items[i];
                if(p.ifToken(si.name.c_str()))
                {
                    a = nodeAddr(a);
                    if(si.offset != 0)
                    {
                        a = nodeOperator(oAdd, a, new NodeConst(si.offset, cbtUShort), true);
                    }
                    if(si.type.arr)
                    {
                        a = nodeConvert(a, si.type, false);
                    }
                    else
                    {
                        Type type = si.type;
                        type.addr++;
                        a = nodeConvert(a, type, false);
                        a = new NodeDeaddr(a);
                    }
                    goto retry;
                }
            }
            p.syntaxError();
        }
        if(p.ifToken("["))
        {
            NodeVar* i = readVar(-1);
            //! Проверка типов
            p.needToken("]");
            if(i->nodeType == ntConstI && i->cast<NodeConst>()->value == 0)
            {
                delete i;
            }
            else
            {
                a = nodeOperator(oAdd, i, a);
            }
            a = new NodeDeaddr(a);
            continue;
        }
        if(p.ifToken("++")) { a = nodeMonoOperator(a, moPostInc); continue; }
        if(p.ifToken("--")) { a = nodeMonoOperator(a, moPostDec); continue; }
        break;
    }

    // Вычисление моно операторов
    for(int i=mo.size()-1; i>=0; i--)
        a = nodeMonoOperator(a, mo[i]);

    return a;
}

//---------------------------------------------------------------------------------------------------------------------

static const char* operators [] = { "/", "%", "*", "+", "-", "<<", ">>", "<", ">", "<=", ">=", "==", "!=", "&",  "^",  "|", "&&",  "||", "?", "=", "+=", "-=", "*=", "/=", "%=", ">>=", "<<=", "&=", "^=", "|=", 0 };
static int         operatorsP[] = { 12,  12,  12,  11,  11,  10,   10,   9,   9,   9,     9,    8,   8,    7,    6,    5,   3,     2,    1,   0,   0,    0,    0,    0,    0,    0,     0,     0,    0,    0       };
static Operator    operatorsI[] = { oDiv,oMod,oMul,oAdd,oSub,oShl, oShr, oL,  oG,  oLE,  oGE,  oE,   oNE,  oAnd, oXor, oOr, oLAnd, oLOr, oIf, oSet,oSAdd,oSSub,oSMul,oSDiv,oSMod,oSShr, oSShl, oSAnd,oSXor,oSOr    };

Operator Parser::findOperator(int level, int& l)
{
    for(int j=0; operators[j] && operatorsP[j] > level; j++)
        if(p.ifToken(operators[j]))
        {
            l = operatorsP[j];
            return operatorsI[j];
        }
    return oNone;
}

//---------------------------------------------------------------------------------------------------------------------

NodeVar* Parser::readVar(int level)
{
    // Чтение аргумента
    NodeVar* a = bindVar();

    // Чтение оператора
    for(;;)
    {
        int l=0;
        Operator o = findOperator(level, l);
        if(o == oNone) break;

        if(o==oIf)
        {
            NodeVar* t = readVar();
            p.needToken(":");
            NodeVar* f = readVar();
            a = nodeOperator(oIf, t, f, false, a);
            continue;
        }

        if(o==oLAnd || o==oLOr) l--;

        NodeVar* b = readVar(l);

        a = nodeOperator(o, a, b);
    }

    return a;
}

//---------------------------------------------------------------------------------------------------------------------
// Чтение константы и преобразование её к определенному типу

uint32_t Parser::readConst(const Type& to)
{
    NodeVar* n = nodeConvert(readVar(), to, true);
    if(n->nodeType != ntConstI) p.syntaxError("Ожидается константа"); //! ntConstS то же нужно здесь
    uint32_t value = n->cast<NodeConst>()->value;
    delete n;
    return value;
}

//---------------------------------------------------------------------------------------------------------------------
// Чтение константы и преобразование её к определенному типу

uint16_t Parser::readConstU16()
{
    return readConst(cbtUShort);
}

//---------------------------------------------------------------------------------------------------------------------

void Parser::readModifiers(Type& t)
{
    while(p.ifToken("*")) t.addr++;
}

//---------------------------------------------------------------------------------------------------------------------

void Parser::parseStruct(Struct& s, int m)
{
    do
    {
        Type type0;
        const char* xx[] = { "struct", "union", 0 };
        unsigned su;
        if(p.ifToken(xx, su))
        {
            Struct* sii = parseStruct3(su);
            if(p.ifToken(";"))
            {
                Struct& s1 = *sii;
                int ss = s.items.size();
                for(unsigned int j=0; j<s1.items.size(); j++)
                    s.items.push_back(s1.items[j]);
                if(m==0)
                    for(unsigned int i=ss; i<s.items.size(); i++)
                        s.items[i].offset += s.size;
                int ts = s1.size;
                if(m==0) s.size += ts; else if(s.size < ts) s.size = ts;
                continue;
            }
            type0.baseType = cbtStruct;
            type0.s = sii;
        }
        else
        {
            type0 = readType(true);
        }
    do {
      s.items.push_back(StructItem());
      StructItem& si = s.items.back();
      Type type = type0;
      readModifiers(type);
      si.type = type;
      si.offset = m==0 ? s.size : 0;
      si.name = p.needIdent();
      if(si.type.arr==0 && p.ifToken("[")) {
        si.type.arr = readConstU16();
        p.needToken("]");
        if(si.type.arr<=0) throw std::runtime_error("struct size");
      }
      int ts = si.type.size();
      if(m==0) s.size += ts; else if(s.size < ts) s.size = ts;
      if(si.type.arr) si.type.addr++;
    } while(p.ifToken(","));
    p.needToken(";");
  } while(!p.ifToken("}"));
}

//---------------------------------------------------------------------------------------------------------------------

Struct* Parser::parseStruct3(int m)
{
    world.structs.push_back(Struct());
    Struct& s1 = world.structs.back();
    s1.size = 0;
    if(!p.ifToken("{"))
    {
        std::string new_name = p.needIdent();
        for(std::list<Struct>::iterator i=world.structs.begin(); i!=world.structs.end(); i++)
            if(i->name == new_name)
                p.syntaxError("Имя уже испольузется");
        s1.name = new_name;
        p.needToken("{");
    }
    else
    {
        char name[256];
        snprintf(name, sizeof(name), "?%u", world.userStructCnt++);
        s1.name = name;
    }
    parseStruct(s1, m);
    return &s1;
}

//---------------------------------------------------------------------------------------------------------------------
// Чтение типа данных

Type Parser::readType(bool error)
{
    p.ifToken("const"); //! Учесть

    if(p.ifToken("void"    )) return cbtVoid;
    if(p.ifToken("uint8_t" )) return cbtUChar;
    if(p.ifToken("uint16_t")) return cbtUShort;
    if(p.ifToken("uint32_t")) return cbtULong;
    if(p.ifToken("int8_t"  )) return cbtUChar;
    if(p.ifToken("int16_t" )) return cbtUShort;
    if(p.ifToken("int32_t" )) return cbtULong;

    // Простые типы данных
    bool u = p.ifToken("unsigned");
    if(p.ifToken("char" )) return u ? cbtUChar  : cbtChar;
    if(p.ifToken("short")) return u ? cbtUShort : cbtShort;
    if(p.ifToken("int"  )) return u ? cbtUShort : cbtShort;
    if(p.ifToken("long" )) return u ? cbtULong  : cbtLong;
    if(u) return cbtUShort;

    // typedef-ы
    for(std::list<Typedef>::iterator i=world.typedefs.begin(); i!=world.typedefs.end(); i++)
        if(p.ifToken(i->name.c_str()))
            return i->type;

    // Читаем struct или union
    const char* xx[] = { "struct", "union", 0 };
    unsigned su;
    if(p.ifToken(xx,  su)) {
        int i=0;
        for(std::list<Struct>::iterator s=world.structs.begin(); s!=world.structs.end(); s++, i++) {
            if(p.ifToken(s->name.c_str())) {
                if(p.ifToken("{")) p.syntaxError("Структура с таким именем уже определена");
                Type type;
                type.baseType = cbtStruct;
                type.s = &*s;
                return type;
            }
        }
        Struct* s = parseStruct3(su);

        // Возврат структуры
        Type type;
        type.baseType = cbtStruct;
        type.s = s;
        return type;
    }

    if(error) p.syntaxError();
    return cbtError;
}

//---------------------------------------------------------------------------------------------------------------------

Node* Parser::readCommand1()
{
    // Пустая команда
    if(p.ifToken(";")) return 0;

    // Надо обработать перед меткой
    if(p.ifToken("default"))
    {
        if(!lastSwitch) p.syntaxError("default без switch");
        p.needToken(":");
        NodeLabel* n = new NodeLabel;
        if(!lastSwitch->setDefault(n)) p.syntaxError("default уже был");
        return n;
    }

    // Метка
    ::Parser::Label pl;
    p.getLabel(pl);
    if(p.cursor[0]==':' && p.ifToken(ttWord))
    {
        std::string t = p.loadedText;
        if(p.ifToken(":")) {
            //! w.label(bindUserLabel(t, true));
        } else {
            p.jump(pl, false);
        }
    }

  /*
  if(p.ifToken("goto")) {
    const char* l = p.needIdent();
    w.jmp(bindUserLabel(l, false));
    p.needToken(";");
    return;
  }
  */

    if(p.ifToken("{"))
    {
        return readBlock();
    }

    if(p.ifToken("break"))
    {
        if(breakLabel == 0) p.syntaxError("break без for, do, while, switch");
        p.needToken(";");
        return world.allocJmp(breakLabel);
    }

    if(p.ifToken("continue"))
    {
        if(continueLabel == 0) p.syntaxError("continue без for, do, while");
        p.needToken(";");
        return world.allocJmp(continueLabel);
    }

    if(p.ifToken("while"))
    {
        // Выделяем метки
        NodeLabel* oldBreakLabel = breakLabel; breakLabel = new NodeLabel;
        NodeLabel* oldContinueLabel = continueLabel; continueLabel = new NodeLabel;
        // Код
        Block b;
        p.needToken("(");
        b << continueLabel;
        NodeVar* cond = readVar();
        p.needToken(")");
        Node* body = readCommand();
        if(body)
        {
            b << world.allocJmp(breakLabel, cond, true);
            b << body;
            b << world.allocJmp(continueLabel);
        }
        else
        {
            // Если тела нет, то можно обойтись без jmp
            b << world.allocJmp(continueLabel, cond, false);
        }
        b << breakLabel;
        breakLabel    = oldBreakLabel;
        continueLabel = oldContinueLabel;
        return b.first;
    }

    if(p.ifToken("do"))
    {
        NodeLabel* oldBreakLabel = breakLabel; breakLabel = new NodeLabel;
        NodeLabel* oldContinueLabel = continueLabel; continueLabel = new NodeLabel;
        Block b;
        b << continueLabel;
        b << readCommand();
        p.needToken("while");
        p.needToken("(");
        std::string r;
        getRemark(r); //! Попадает только первая строка!
        NodeVar* vv = readVar(-1);
        if(vv) vv->remark.swap(r);
        b << world.allocJmp(continueLabel, vv, false);
        p.needToken(")");
        breakLabel    = oldBreakLabel;
        continueLabel = oldContinueLabel;
        return b.first;
    }

    if(p.ifToken("for"))
    {
        NodeLabel* oldBreakLabel = breakLabel; breakLabel = new NodeLabel;
        NodeLabel* oldContinueLabel = continueLabel; continueLabel = new NodeLabel;
        NodeLabel* startLabel = new NodeLabel;
        p.needToken("(");
        Block a;
        if(!p.ifToken(";"))
        {
            do
            {
                a << readVar();
            }
            while(p.ifToken(","));
            p.needToken(";");
        }
        NodeVar* cond = 0;
        if(!p.ifToken(";"))
        {
            cond = readVar(-1);
            p.needToken(";");
        }
        Block c;
        if(!p.ifToken(")"))
        {
            do
            {
                c << readVar();
            }
            while(p.ifToken(","));
            p.needToken(")");
        }

        Node* cmd = readCommand();

        a << startLabel;
        if(!c.first) a << continueLabel;
        if(cond) a << world.allocJmp(breakLabel, cond, true);
        a << cmd;
        if(c.first)
        {
            a << continueLabel;
            a << c.first;
            a << world.allocJmp(startLabel);
        }
        else
        {
            a << world.allocJmp(startLabel);
        }
        a << breakLabel;
        breakLabel    = oldBreakLabel;
        continueLabel = oldContinueLabel;
        return a.first;
    }

    if(p.ifToken("if"))
    {
        p.needToken("(");
        NodeVar* cond = readVar();
        p.needToken(")");
        Node* t = readCommand();
        Node* f = 0;
        if(p.ifToken("else")) f = readCommand();
        return world.allocIf(cond, t, f);
    }

    if(p.ifToken("switch"))
    {
        NodeLabel* oldBreakLabel = breakLabel;
        breakLabel = new NodeLabel;
        outOfMemory(breakLabel);
        p.needToken("(");
        NodeVar* var = readVar();
        p.needToken(")");
        p.needToken("{");
        NodeSwitch* s = new NodeSwitch(var);
        outOfMemory(s);
        NodeSwitch* saveSwitch = lastSwitch;
        lastSwitch = s;
        Block b;
        b << readBlock();
        b << breakLabel;
        s->body = b.first;
        if(!s->defaultLabel) s->defaultLabel = breakLabel;
        lastSwitch = saveSwitch;
        breakLabel = oldBreakLabel;
        return s;
    }

    if(p.ifToken("case"))
    {
        if(!lastSwitch) p.syntaxError("case без switch");
        uint32_t i = readConst(lastSwitch->var->dataType);
        p.needToken(":");
        NodeLabel* n = new NodeLabel;
        if(!lastSwitch->addCase(i, n)) p.syntaxError("case уже был");
        return n;
    }

    if(p.ifToken("return"))
    {
        if(curFn->retType.isVoid()) return outOfMemory(new NodeReturn(0));
        return outOfMemory(new NodeReturn(nodeConvert(readVar(), curFn->retType, true))); //! Если сработает outOfMemory, то будет утечка памяти
    }

    // Описание переменной и её инициализация
    bool reg = p.ifToken("register");
    Type t = readType(false);
    if(t.baseType != cbtError)
    {
        Block b;
        do
        {
            // Модификаторы типа
            Type t1 = t;
            readModifiers(t1);

            // Имя
            const char* n = p.needIdent();
            if(!checkStackUnique(n)) p.syntaxError("Дубликат");

            // Массив
            if(p.ifToken("["))
            {
                t1.arr = readConstU16();
                if(t1.arr <= 0) throw std::runtime_error("[]");
                p.needToken("]");
                t1.addr++;
            }

            // Адрес в стеке
            unsigned stackOff = (stackVars.size()==0 || stackVars.back().arg) ? 0 : (stackVars.back().addr + stackVars.back().type.size());
            if(t1.size()>=2) stackOff = (stackOff + 1 ) & ~1;

            // Размер стека
            unsigned stackSize = ((stackOff + t1.size() + 1) & ~1);
            if(curFn->stackSize < stackSize) curFn->stackSize = stackSize;

            // Сохраняем информацию
            stackVars.push_back(StackVar());
            StackVar& v = stackVars.back();
            v.name = n;
            v.addr = stackOff;
            v.arg  = false;
            v.type = t1;
            v.reg  = reg;

            // Инициализация
            if(p.ifToken("=")) b << nodeOperator(oSet, getStackVar(v), nodeConvert(readVar(-1), t1, true));
        } while(p.ifToken(","));
        p.needToken(";");
        return b.first;
    }
    if(reg) p.syntaxError();

    // Остальные команды
    Block b;
    do { b << readVar(); } while(p.ifToken(","));
    p.needToken(";");
    return b.first;
}

//---------------------------------------------------------------------------------------------------------------------

void Parser::getRemark(std::string& out)
{
    const char* c = p.prevCursor;
    while(c > p.firstCursor && c[-1]!='\r' && c[-1]!='\n') c--;
    const char* e = strchr(c, '\r');
    const char* e2 = strchr(c, '\n');
    if(e==0 || e>e2) e=e2;
    int l;
    if(e==0) l = strlen(c); else l = e-c;
    out = i2s(p.line) + " " + std::string(c, l);
}

//---------------------------------------------------------------------------------------------------------------------
// Читаем команду

Node* Parser::readCommand()
{
    std::string r;
    getRemark(r); //! Попадает только первая строка!
    Node* n = readCommand1();
    if(n) n->remark.swap(r);
    return n;
}

//---------------------------------------------------------------------------------------------------------------------
// Читаем блок команд { ... }

Node* Parser::readBlock()
{
    int prevStackSize = stackVars.size();
    Block b;
    while(!p.ifToken("}")) b << readCommand();
    stackVars.resize(prevStackSize);
    return b.first;
}

//---------------------------------------------------------------------------------------------------------------------

Function* Parser::parseFunction(Type& retType, const std::string& name)
{
    stackVars.clear();

    Function* f = world.findFunction(name);

    std::vector<FunctionArg> argTypes;
    if(!p.ifToken(")"))
    {
        unsigned off = 2, n = 0;
        do
        {
            if(p.ifToken("...")) break; //! Не учитывается

            Type t = readType(true);
            readModifiers(t);
            if(t.baseType==cbtVoid && t.addr==0) break;

            stackVars.push_back(StackVar());
            StackVar& v = stackVars.back();

            if(p.ifToken(ttWord))
            {
                if(!checkStackUnique(p.loadedText)) p.syntaxError(("Имя используется " + (std::string)p.loadedText).c_str());
                v.name = p.loadedText;
            }
            if(p.ifToken("["))
            {
                p.needToken("]");
                t.addr++;
            }

            unsigned reg = 0;

            if(p.ifToken("@"))
            {
                const char* args[] = { "r0", "r1", "r2", "r3", "r4", "r5", 0  };
                reg = p.needToken(args) + 1;
            }

            argTypes.push_back(FunctionArg(n, t, reg));

            v.addr = off;
            v.arg  = true;
            v.type = t;

            off += v.type.size();
            n++;
        } while(p.ifToken(","));
        p.needToken(")");
    }

    if(!f)
    {
        f = world.addFunction(name.c_str());
        f->args    = argTypes;
        f->retType = retType;
    }
    else
    {
        //!!!!!!!!!! Сравнить типы
    }

    if(p.ifToken("@"))
    {
        p.needToken("emt");
        p.needToken(ttInteger);
        f->call_type = 1;
        f->call_arg = p.tokenNum;
        if(p.ifToken(","))
        {
            const char* args[] = { "r0", "r1", "r2", "r3", "r4", "r5", 0  };
            f->reg = p.needToken(args) + 1;
        }
    }

    if(p.ifToken(";")) return 0; // proto

    if(f->call_type) p.syntaxError();

    p.needToken("{");

    curFn = f;
    f->root = readBlock();
    f->parsed = true;
    curFn = 0;

    return f;
}

//---------------------------------------------------------------------------------------------------------------------

//! Нет никакого контроля типов
//! Нет указателей, arr
//! Нет длины!
void Parser::arrayInit(std::vector<uint8_t>& data, Type& type)
{    
    if(type.addr>0 && p.ifToken("{"))
    {
        type.addr--;
        type.arr = 0;
        while(1)
        {
            if(p.ifToken("}")) break;
            arrayInit(data, type);
            if(p.ifToken("}")) break;
            p.needToken(",");
        }
        return;
    }

    if(type.addr==0 && type.arr==0 && type.baseType==cbtStruct && type.s)
    {
        Struct& s = *type.s;
        p.needToken("{");
        for(unsigned int u=0; u<s.items.size(); u++)
        {
            if(u>0) p.needToken(",");
            arrayInit(data, s.items[u].type);
        }
        p.needToken("}");
    }

    if(p.ifToken(ttString2))
    {
        if(type.addr != 1 || type.sizeElement() != 1) p.syntaxError("Ожидается строка");
        size_t s = 0;
        do {
            data.resize(s + p.loadedTextSize + 1);
            memcpy(&data[s], p.loadedText, p.loadedTextSize + 1);
            s += p.loadedTextSize;
        } while(p.ifToken(ttString2));
        return;
    }

    NodeVar* n = readVar(-1);
    if(n->nodeType != ntConstI) p.syntaxError("Ожидается константа");
    uint32_t v = n->cast<NodeConst>()->value;
    delete n;

    uint8_t* vp = (uint8_t*)&v;
    switch(type.addr ? cbtUShort : type.baseType)
    {
        case cbtUChar:  case cbtChar:  data.insert(data.end(), vp, vp+1); break;
        case cbtUShort: case cbtShort: data.insert(data.end(), vp, vp+2); break;
        case cbtULong:  case cbtLong:  data.insert(data.end(), vp, vp+4); break;
        default: p.syntaxError("Инициализация этого типа не поддерживается");
    }
}

//---------------------------------------------------------------------------------------------------------------------

void Parser::parse2()
{
    bool typedef1  = p.ifToken("typedef");
    bool extren1   = !typedef1 && p.ifToken("extern");
    bool static1   = !typedef1 && !extren1 && p.ifToken("static");
    bool register1 = !typedef1 && !extren1 && p.ifToken("register");

    Type fnType1  = readType(true);

    while(1)
    {
        Type type = fnType1;
        readModifiers(type);

        std::string name = p.needIdent();

        if(p.ifToken("("))
        {
            if(typedef1) p.syntaxError("typedef не поддерживает функции");
            parseFunction(type, name);
            break;
        }

        if(type.arr==0 && p.ifToken("["))
        {
            if(p.ifToken("]")) {
                type.arr = 1;
            } else {
                type.arr = readConstU16();
                if(type.arr <= 0) throw std::runtime_error("[]");
                p.needToken("]");
            }
            type.addr++;
            if(p.ifToken("["))
            {
                type.addr++;
                type.arr2 = readConstU16();
                if(type.arr2 <= 0) throw std::runtime_error("[]");
                p.needToken("]");
                if(p.ifToken("[")) {
                    type.addr++;
                    type.arr3 = readConstU16();
                    if(type.arr3 <= 0) throw std::runtime_error("[]");
                    p.needToken("]");
                }
            }
        }

        if(typedef1)
        {
            if(!world.checkUnique(name)) p.syntaxError("Имя уже используется");
            world.addTypedef(name.c_str(), type);
        }
        else
        {
            if(!extren1)
            {
                if(!world.checkUnique(name)) p.syntaxError("Имя уже используется");
            } //!!! Сравнить типы данных

            GlobalVar* g = world.addGlobalVar(name.c_str());
            g->name    = name;
            g->type    = type;
            g->extren1 = extren1;
            g->reg     = register1;
            if(!extren1)
            {
                if(p.ifToken("="))
                    arrayInit(g->data, g->type);

                unsigned s = g->data.size();
                unsigned ns = type.size();
                if(s < ns)
                {
                    g->data.resize(ns);
                    memset(&g->data[s], 0, ns-s);
                }
            }
        }

        if(p.ifToken(";")) break;
        p.needToken(",");
    }
}

//---------------------------------------------------------------------------------------------------------------------

static const char* c_operators[] = {
  ".", "..", "...", "++", "--", "/", "%", "*", "$", "+", "-",
  "<<", ">>", "<", ">", "<=", ">=", "==", "!=", "&",  "^",  "|", "&&",
  "||", "?", "=", "+=", "-=", "*=", "/=", "%=", ">>=", "<<=", "&=", "^=", "|=",
  "->", "//", "/*", "//", 0
};

static const char* c_remark[] = {
    "//", 0
};

void Parser::start(unsigned step)
{
    ::Parser::Config oldCfg = p.cfg;

    p.cfg.cescape        = true;
    p.cfg.eol            = false;
    p.cfg.operators      = &c_operators[0];
    p.cfg.remark         = c_remark;
    p.cfg.caseSel        = true;
    p.cfg.decimalnumbers = true;

    p.needToken(ttEol);

    if(step==0)
    {
        while(p.token!=ttOperator || p.tokenText[0]!='}')
            parse2();
    }
    else
    {
        // Во время второго прохода мы просто пропускаем исходники
        for(unsigned l=0;;)
        {
            if(p.ifToken(ttEof)) break;
            if(l==0 && p.token==ttOperator && p.tokenText[0]=='}') break;
            if(p.ifToken("}")) { if(l==0) throw; l--; continue; }
            if(p.ifToken("{")) { l++; continue; }
            p.nextToken();
        }
    }

    p.cfg = oldCfg;
    p.nextToken();
}

}
