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

bool Parser::p_ifToken(const std::map<std::string,int>& hash, int& n)
{
    const std::map<std::string,int>::const_iterator vv = hash.find(p.tokenText);
    if(vv != hash.end())
    {
        n = vv->second;
        p.nextToken();
        return true;
    }
    return false;
}

//---------------------------------------------------------------------------------------------------------------------

bool Parser::p_ifToken(const std::vector<std::string>& strs, int& o) {
    unsigned i;
    for(i=0; i<strs.size(); i++)
        if(p.ifToken(strs[i].c_str())) {
            o=i;
            return true;
        }
    return false;
}

//---------------------------------------------------------------------------------------------------------------------

bool Parser::p_ifToken(const std::vector<Var>& a, int& o) {
    unsigned i;
    for(i=0; i<a.size(); i++)
        if(p.ifToken(a[i].name.c_str())) {
            o=i;
            return true;
        }
    return false;
}

//---------------------------------------------------------------------------------------------------------------------

bool Parser::p_ifToken(const std::list<Function>& a, int& o) {
    unsigned i=0;
    std::list<Function>::const_iterator f = a.begin();
    for(; f!=a.end(); f++, i++)
        if(p.ifToken(f->name.c_str())) {
            o=i;
            return true;
        }
    return false;
}

//---------------------------------------------------------------------------------------------------------------------

NodeIf* Parser::allocNodeIf(NodeVar* _cond, Node* _t, Node* _f) {
  NodeIf* n = new NodeIf;
  outOfMemory(n);
  n->cond = _cond;
  n->t = _t;
  n->f = _f;
  return n;
}

//---------------------------------------------------------------------------------------------------------------------

NodeVar* Parser::getStackVar(Var& x)
{
    Type t = x.type;
    if(!t.arr) t.addr++;
    NodeVar* c = new NodeConst(x.stackOff, t); // curFn->name+"_"+x.name
    NodeVar* r = new NodeOperator(c->dataType, oAdd, c, new NodeSP, 0);
    if(!t.arr) c = new NodeDeaddr(r);
    return c;
}

//---------------------------------------------------------------------------------------------------------------------

NodeJmp* Parser::allocJmp(NodeLabel* _label, NodeVar* _cond, bool _ifZero)
{
    // Константа в условии
    if(_cond->nodeType == ntConstI)
    {
        NodeConst* nc = _cond->cast<NodeConst>();
        bool check = nc->value != 0;
        if(_ifZero) check = !check;
        delete _cond;
        _cond = 0;
        if(!check) return 0;
    }

    NodeJmp* j = new NodeJmp;
    outOfMemory(j);
    j->cond   = _cond;
    j->label  = _label;
    j->ifZero = _ifZero;
    return j;
}

//---------------------------------------------------------------------------------------------------------------------

NodeJmp* Parser::allocJmp(NodeLabel* _label)
{
    NodeJmp* j = new NodeJmp;
    outOfMemory(j);
    j->label  = _label;
    return j;
}

//---------------------------------------------------------------------------------------------------------------------

NodeVar* Parser::nodeConvert(NodeVar* x, Type type)
{
    // Преобразовывать не надо
    if(type == x->dataType) return x;

    // Константы преобразуются налету
    if(x->isConst())
    {
        if(x->nodeType==ntConstI)
        {
            if(type.is8()) x->cast<NodeConst>()->value &= 0xFF; else
            if(type.is16()) x->cast<NodeConst>()->value &= 0xFFFF;
        }
        x->dataType = type;
        return x;
    }

    // 8 битные арифметические операции
    if(x->nodeType==ntOperator && type.is8())
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
    }

    // Преобразовывать надо
    return new NodeConvert(x, type);
}

//---------------------------------------------------------------------------------------------------------------------

NodeVar* Parser::bindVar_2()
{
    // Чтение возможных значений
    if(p.ifToken(ttString2)) {
        std::string buf;
        buf += p.loadedText;
        while(p.ifToken(ttString2))
            buf += p.loadedText;
        Type type;
        type.addr = 1;
        type.baseType = cbtChar;
        return new NodeConst(world.regString(buf.c_str()), type);
    }
    if(p.ifToken(ttString1)) {
        return new NodeConst((unsigned char)p.loadedText[0], cbtChar);
    }
    if(p.ifToken(ttInteger)) {
        return new NodeConst(p.loadedNum, cbtShort); //! Должен быть неопределенный размер
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
            return nodeConvert(bindVar(), type);
        }
        NodeVar* a = readVar(-1);
        p.needToken(")");
        return a;
    }

    // Стековая переменная
    int i;
    if(p_ifToken(stackVars, i))
    {
        Var& x = stackVars[i];
        return getStackVar(x);

    }

    // Глобальная переменная
    if(p_ifToken(world.globalVars, i)) {
        Type g = world.globalVarsT[i];

        // Если требуется подключить внешний файл
        //if(!g.needInclude.empty()) {
        //    needFile(g.needInclude.c_str());
        //    clearString(g.needInclude);
        //}
        //if(type.baseType == cbtStruct) type.place = pConstStrRef16;

        // Эта переменная не требует DEADDR
        if(g.arr) return new NodeConst(world.globalVars[i], g);

        // Переменная описана без адреса. int как int. Но в реальности надо обернуть все переменные в DEADDR
        g.addr++;
        return new NodeDeaddr(new NodeConst(world.globalVars[i], g));
    }

    // Это функция
    if(p_ifToken(world.functions, i))
    {
        Function& f = get(world.functions, i);

        // Для вызова функции надо подключить файл
        //if(!f.needInclude.empty()) {
        //    needFile(f.needInclude.c_str());
        //    clearString(f.needInclude);
        //}

        p.needToken("(");

//    if(!f.retType.isVoid() && !f.retType.isStackType()) {
//      string tmpVar = "tmp" + i2s(tmpCnt++);
//      asm_decl(::fnName, tmpVar, f.retType);
//      new NodeGlobalVar(::fnName, tmpVar, type);
//    }

        // Чтение аргументов
        std::vector<NodeVar*> args;
        for(unsigned int j=0; j<f.args.size(); j++) {
            if(j>0) p.needToken(",");
            args.push_back(nodeConvert(readVar(-1), f.args[j]));
        }
        p.needToken(")");

        // Вызов функции
        if(f.callAddr) return new NodeCall(f.addr, f.retType, args);
                  else return new NodeCall(f.name, f.retType, args);
    }

    p.syntaxError();
    return 0;
}

//---------------------------------------------------------------------------------------------------------------------

NodeVar* Parser::nodeOperator2(Type type, Operator o, NodeVar* a, NodeVar* b, NodeVar* cond) {
  // Сравнение дает флаги
  switch((unsigned)o) {
    case oE:
    case oNE:
    case oG:
    case oGE:
    case oL:
    case oLE:
      type.baseType = cbtShort;
      type.addr = 0;
      break;
  }

  if(o==oIf) {
    //! Проверить условие на константность!  || cond->nodeType == ntConstS

    if(cond->nodeType == ntConstI) {
      if(cond->cast<NodeConst>()->value) {
        delete cond;
        delete b;
        return a;
      } else {
        delete cond;
        delete a;
        return b;
      }
    }

    return new NodeOperator(type, o, a, b, cond);
  }

    // Вычисление операция между константами на экрапе компиляции
    if((a->nodeType==ntConstI || a->nodeType==ntConstS) && (b->nodeType==ntConstI || b->nodeType==ntConstS))
    {
        NodeConst* ac = a->cast<NodeConst>(), *bc = b->cast<NodeConst>();
        NodeVar* x = 0;
        if(a->nodeType==ntConstI && b->nodeType==ntConstI)
        {
            switch(o)
            {
                case oAdd: x = new NodeConst(ac->value +  bc->value, type); break;
                case oSub: x = new NodeConst(ac->value -  bc->value, type); break;
                case oAnd: x = new NodeConst(ac->value &  bc->value, type); break;
                case oXor: x = new NodeConst(ac->value ^  bc->value, type); break;
                case oMul: x = new NodeConst(ac->value *  bc->value, type); break;
                case oDiv: x = new NodeConst(ac->value /  bc->value, type); break;
                case oShr: x = new NodeConst(ac->value >> bc->value, type); break;
                case oShl: x = new NodeConst(ac->value << bc->value, type); break;
                case oNE:  x = new NodeConst(ac->value != bc->value ? 1 : 0, type); break;
                case oE:   x = new NodeConst(ac->value == bc->value ? 1 : 0, type); break;
                case oGE:  x = new NodeConst(ac->value >= bc->value ? 1 : 0, type); break;
                case oG:   x = new NodeConst(ac->value >  bc->value ? 1 : 0, type); break;
                case oLE:  x = new NodeConst(ac->value <= bc->value ? 1 : 0, type); break;
                case oL:   x = new NodeConst(ac->value <  bc->value ? 1 : 0, type); break;
                case oOr:  x = new NodeConst(ac->value | bc->value); break;
                default: assert(0);
            }
        }
        else
        {
            switch(o) {
                case oAdd: x = new NodeConst("(" + ac->name() + ")+("  + bc->name() + ")", type); break;
                case oSub: x = new NodeConst("(" + ac->name() + ")-("  + bc->name() + ")", type); break;
                case oAnd: x = new NodeConst("(" + ac->name() + ")&("  + bc->name() + ")", type); break;
                case oXor: x = new NodeConst("(" + ac->name() + ")^("  + bc->name() + ")", type); break;
                case oMul: x = new NodeConst("(" + ac->name() + ")*("  + bc->name() + ")", type); break;
                case oDiv: x = new NodeConst("(" + ac->name() + ")/("  + bc->name() + ")", type); break;
                case oShr: x = new NodeConst("(" + ac->name() + ")>>(" + bc->name() + ")", type); break;
                //! Дописать
                default: assert(0);
            }
        }
        delete a;
        delete b;
        return x;
    }

    // *** ОПТИМИЗАЦИЯ ***

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
    if(o == oAnd) b = nodeMonoOperator(b, moXor); //!!! Приоритет у константы или регистра (т.е. прошлое вычисление дало регистр)

    return new NodeOperator(type, o, a, b, cond);
}

//---------------------------------------------------------------------------------------------------------------------

NodeVar* Parser::nodeOperator(Operator o, NodeVar* a, NodeVar* b, bool noMul, NodeVar* cond)
{
    switch((unsigned)o)  //! Убрать
    {
        // Заменяем операторы += на простые операторы
        case oSAdd: return nodeOperator(oSet, a, nodeConvert(nodeOperator(oAdd, a, b, noMul, 0), a->dataType), noMul, 0);
        case oSSub: return nodeOperator(oSet, a, nodeConvert(nodeOperator(oSub, a, b, noMul, 0), a->dataType), noMul, 0);
        case oSMul: return nodeOperator(oSet, a, nodeConvert(nodeOperator(oMul, a, b, noMul, 0), a->dataType), noMul, 0);
        case oSDiv: return nodeOperator(oSet, a, nodeConvert(nodeOperator(oDiv, a, b, noMul, 0), a->dataType), noMul, 0);
        case oSMod: return nodeOperator(oSet, a, nodeConvert(nodeOperator(oMod, a, b, noMul, 0), a->dataType), noMul, 0);
        case oSShl: return nodeOperator(oSet, a, nodeConvert(nodeOperator(oShl, a, b, noMul, 0), a->dataType), noMul, 0);
        case oSShr: return nodeOperator(oSet, a, nodeConvert(nodeOperator(oShr, a, b, noMul, 0), a->dataType), noMul, 0);
        case oSAnd: return nodeOperator(oSet, a, nodeConvert(nodeOperator(oAnd, a, b, noMul, 0), a->dataType), noMul, 0);
        case oSXor: return nodeOperator(oSet, a, nodeConvert(nodeOperator(oXor, a, b, noMul, 0), a->dataType), noMul, 0);
        case oSOr:  return nodeOperator(oSet, a, nodeConvert(nodeOperator(oOr,  a, b, noMul, 0), a->dataType), noMul, 0);
    }

    Type type;

    // Сложение указателя и числа
    if(o==oAdd)
    {
        if(a->dataType.addr!=0 && b->dataType.addr==0 && b->dataType.is8_16()) {
            if(!noMul) b = nodeOperator(oMul, b, new NodeConst(a->dataType.sizeElement()));
            return nodeOperator2(a->dataType, o, a, b, cond);
        }
        if(a->dataType.addr==0 && a->dataType.is8_16() && b->dataType.addr!=0) {
            if(!noMul) a = nodeOperator(oMul, a, new NodeConst(b->dataType.sizeElement()));
            return nodeOperator2(b->dataType, o, a, b, cond);
        }
    }

    // Вычитание числа из указателя
    if(o==oSub && a->dataType.addr!=0 && b->dataType.addr==0 && b->dataType.is8_16()) {
        if(!noMul) b = nodeOperator(oMul, b, new NodeConst(a->dataType.sizeElement()));
        return nodeOperator2(a->dataType, o, a, b, cond);
    }

    // Вычитание указателя из указателя
    if(o==oSub && a->dataType.addr!=0 && a->dataType.addr==b->dataType.addr && a->dataType.baseType==b->dataType.baseType) {
        Type stdType;
        stdType.baseType = cbtUShort;
        stdType.addr = 0;
        NodeVar* n = nodeOperator2(stdType, o, a, b, cond);
        if(!noMul) n = nodeOperator(oDiv, n, new NodeConst(a->dataType.sizeElement()));
        return n;
    }

    // Сравнение указателей
    if((o==oE || o==oNE || o==oL || o==oG || o==oLE || o==oGE) && a->dataType.addr!=0 && a->dataType.addr==b->dataType.addr && a->dataType.baseType==b->dataType.baseType)
    {
        return nodeOperator2(a->dataType, o, a, b, cond); // Тип не важен, но изменится в функции
    }

    // ? для указателей
    //! надо бы привести указатели к VOID* для ?
    if(o==oIf && a->dataType.addr!=0 && a->dataType.addr==b->dataType.addr && a->dataType.baseType==b->dataType.baseType) {
        return nodeOperator2(a->dataType, o, a, b, cond);
    }

    // Преобразование нуля в указатель
    if(a->dataType.addr!=0 && b->nodeType==ntConstI && ((NodeConst*)b)->value==0) {
        return nodeOperator2(a->dataType, o, a, b, cond); // Тип не важен, но изменится в функции
    }
    if(b->dataType.addr!=0 && a->nodeType==ntConstI && ((NodeConst*)a)->value==0) {
        return nodeOperator2(b->dataType, o, a, b, cond); // Тип не важен, но изменится в функции
    }

  // Два условия в LAND и LOR
  if(o==oLAnd || o==oLOr) {
    //if(a->dataType.baseType!=cbtFlags || b->dataType.baseType!=cbtFlags) throw std::runtime_error("LAND и LOR не правильно сформированы");
    return nodeOperator2(a->dataType, o, a, b, cond);
  }

  //!!! Это не совсем правильно, так как OR, AND всегда будут давать 8 битный результат

  // Число приводится к типу второго аргумента.
  Type at = a->dataType, bt = b->dataType;
  if(a->nodeType == ntConstI && b->nodeType != ntConstI) at = b->dataType; else
  if(b->nodeType == ntConstI && a->nodeType != ntConstI) bt = a->dataType;

  Type dataType;
  if(o==oSet) {
    //! Проверить типы!!!

    // Запись
    dataType = at;
  } else {
    if(at.addr != 0 || bt.addr != 0)
      p.syntaxError("Такая операция между указателями невозможна");

    // Преобразование меньшего к большему
    bool sig = at.isSigned();
    bool is16 = at.is16() || bt.is16();
    bool is32 = at.is32() || bt.is32();
    dataType = is32 ? (sig ? cbtLong : cbtULong) : is16 ? (sig ? cbtShort : cbtUShort) : (sig ? cbtChar : cbtUChar);
  }

    // Арифметические операции между 8 битными числами дают 16 битный результат
    if(dataType.is8()) {
        switch((unsigned)o) {
            case oAdd:
            case oSub:
            case oDiv:
            case oMod:
            case oMul:
                dataType = dataType.isSigned() ? cbtShort : cbtUShort;
        }
    }

    if(o != oSet) { //! oSetVoid
        if(a->dataType.baseType != dataType.baseType || a->dataType.addr != dataType.addr) a = nodeConvert(a, dataType);
    }
    if(b->dataType.baseType != dataType.baseType || b->dataType.addr != dataType.addr) b = nodeConvert(b, dataType);

    return nodeOperator2(dataType, o, a, b, cond);
}

NodeVar* Parser::nodeAddr(NodeVar* x)
{
    if(x->nodeType != ntDeaddr) p.syntaxError("addrNode");
    NodeDeaddr* a = ((NodeDeaddr*)x);
    NodeVar* result = a->var;
    //if(a->nodeType==ntConstS && a->cast<NodeConst>()->var) throw std::runtime_error("Нельзя получить адрес регистровой переменной");
    a->var = 0;
    delete a;
    return result;
}

//---------------------------------------------------------------------------------------------------------------------

NodeVar* Parser::nodeMonoOperator(NodeVar* a, MonoOperator o)
{
    if(o == moDeaddr) return new NodeDeaddr(a);

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

    while(true)
    {
    retry:
        if(p.ifToken("->"))
        {
            a = new NodeDeaddr(a);
            goto xx;
        }
        if(p.ifToken("."))
        {
xx:         if(a->dataType.baseType!=cbtStruct || a->dataType.addr!=0 || a->dataType.s==0) throw std::runtime_error("Ожидается структура");
            Struct& s = *a->dataType.s;
            for(unsigned int i=0; i<s.items.size(); i++)
                if(p.ifToken(s.items[i].name.c_str()))
                {
                    a = nodeAddr(a); // new NodeAddr(a);
                    if(s.items[i].offset != 0)
                    {
                        a = nodeOperator(oAdd, a, new NodeConst(s.items[i].offset), true);
                    }
                    Type type = s.items[i].type;
                    if(type.arr)
                    {
                        a = nodeConvert(a, type);
                    }
                    else
                    {
                        type.addr++;
                        a = nodeConvert(a, type);
                        a = new NodeDeaddr(a);
                    }
                    goto retry;
                }
            p.syntaxError();
        }
        if(p.ifToken("["))
        {
            NodeVar* i = readVar(-1);
            //! Проверка типов
            p.needToken("]");
            if(i->nodeType == ntConstI && ((NodeConst*)i)->value == 0)
            {
                delete i;
            }
            else
            {
                a = nodeOperator(oAdd, a, i);
            }
            a = new NodeDeaddr(a);
            continue;
        }
        if(p.ifToken("++")) { a = nodeMonoOperator(a, moPostInc); continue; }
        if(p.ifToken("--")) { a = nodeMonoOperator(a, moPostDec); continue; }
        break;
    }

  // Вычисление моно операторов
  for(int i=mo.size()-1; i>=0; i--) {
    switch(mo[i]) {
      case moDeaddr: a = new NodeDeaddr(a); break;
      case moAddr: a = nodeAddr(a); break;
      default: a = nodeMonoOperator(a, mo[i]);
    }
  }

  return a;
}

//---------------------------------------------------------------------------------------------------------------------

static const char* operators [] = { "/", "%", "*", "+", "-", "<<", ">>", "<", ">", "<=", ">=", "==", "!=", "&",  "^",  "|", "&&",  "||", "?", "=", "+=", "-=", "*=", "/=", "%=", ">>=", "<<=", "&=", "^=", "|=", 0 };
static int         operatorsP[] = { 12,  12,  12,  11,  11,  10,   10,   9,   9,   9,     9,    8,   8,    7,    6,    5,   3,     2,    1,   0,   0,    0,    0,    0,    0,    0,     0,     0,    0,    0       };
static Operator    operatorsI[] = { oDiv,oMod,oMul,oAdd,oSub,oShl, oShr, oL,  oG,  oLE,  oGE,  oE,   oNE,  oAnd, oXor, oOr, oLAnd, oLOr, oIf, oSet,oSAdd,oSSub,oSMul,oSDiv,oSMod,oSShr, oSShl, oSAnd,oSXor,oSOr    };

Operator Parser::findOperator(int level, int& l) {
  for(int j=0; operators[j] && operatorsP[j] > level; j++)
    if(p.ifToken(operators[j])) {
      l = operatorsP[j];
      return operatorsI[j];
    }
  return oNone;
}

//---------------------------------------------------------------------------------------------------------------------

NodeVar* Parser::readVar(int level) {
  // Чтение аргумента
  NodeVar* a = bindVar();

  // Чтение оператора
  while(true) {
    int l=0;
    Operator o = findOperator(level, l);
    if(o == oNone) break;

    if(o==oIf) {
      NodeVar* t = readVar(-1);
      p.needToken(":");
      NodeVar* f = readVar(-1);
      a = nodeOperator(oIf, t, f, false, a);
      continue;
    }

    if(o==oLAnd || o==oLOr) l--;

    NodeVar* b = readVar(l);

    //if(o==oLAnd || o==oLOr) {
      //a = addFlag(a);
      //b = addFlag(b);
    //}

    a = nodeOperator(o, a, b);

    //if(o==oLAnd || o==oLOr) {
    //  a = addFlag(a);
    //}
  }

  return  a;
}

//---------------------------------------------------------------------------------------------------------------------
// Чтение константы и преобразование её к определенному типу

uint32_t Parser::readConst(Type& to)
{
    NodeVar* n = nodeConvert(readVar(-1), to);
    if(n->nodeType != ntConstI) p.syntaxError("Ожидается константа");
    uint32_t value = n->cast<NodeConst>()->value;
    delete n;
    return value;
}

//---------------------------------------------------------------------------------------------------------------------
// Чтение константы и преобразование её к определенному типу

uint16_t Parser::readConstU16()
{
    Type t(cbtUShort);
    return readConst(t);
}

//---------------------------------------------------------------------------------------------------------------------

void Parser::readModifiers(Type& t) {
    while(p.ifToken("*")) t.addr++;
}

//---------------------------------------------------------------------------------------------------------------------

void Parser::parseStruct(Struct& s, int m) {
  do {
    Type type0;
    const char* xx[] = { "struct", "union", 0 };
    int su;
    if(p.ifToken(xx, su)) {
      Struct* sii = parseStruct3(su);
      if(p.ifToken(";")) {
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
    } else {
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
    if(!p.ifToken("{")) {
        s1.name = p.needIdent();
        p.needToken("{");
    } else {
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
    int su;
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
        return allocJmp(breakLabel);
    }

    if(p.ifToken("continue"))
    {
        if(continueLabel == 0) p.syntaxError("continue без for, do, while");
        p.needToken(";");
        return allocJmp(continueLabel);
    }

  if(p.ifToken("while")) {
    // Выделяем метки
    NodeLabel* oldBreakLabel = breakLabel; breakLabel = new NodeLabel(world.intLabels);
    NodeLabel* oldContinueLabel = continueLabel; continueLabel = new NodeLabel(world.intLabels);
    // Код
    Node *first=0, *last=0;
    p.needToken("(");
    Tree::linkNode(first, last, continueLabel);
    NodeVar* cond = readVar(-1);
    p.needToken(")");
    Node* body = readCommand();
    if(body) {
      Tree::linkNode(first, last, allocJmp(breakLabel, cond, true));
      Tree::linkNode(first, last, body);
      Tree::linkNode(first, last, allocJmp(continueLabel));
    } else {
      // Если тела нет, то можно обойтись без jmp
      Tree::linkNode(first, last, allocJmp(continueLabel, cond, false));
    }
    Tree::linkNode(first, last, breakLabel);
    breakLabel    = oldBreakLabel;
    continueLabel = oldContinueLabel;
    return first;
  }

  if(p.ifToken("do")) {
    NodeLabel* oldBreakLabel = breakLabel; breakLabel = new NodeLabel(world.intLabels);
    NodeLabel* oldContinueLabel = continueLabel; continueLabel = new NodeLabel(world.intLabels);
    Node *first=0, *last=0;
    Tree::linkNode(first, last, continueLabel);
    Tree::linkNode(first, last, readCommand());
    p.needToken("while");
    p.needToken("(");

    std::string r;
    getRemark(r); //! Попадает только первая строка!
    NodeVar* vv = readVar(-1);
    if(vv) vv->remark.swap(r);

    Tree::linkNode(first, last, allocJmp(continueLabel, vv, false));
    p.needToken(")");
    breakLabel    = oldBreakLabel;
    continueLabel = oldContinueLabel;
    return first;
  }

    if(p.ifToken("for"))
    {
        NodeLabel* oldBreakLabel = breakLabel; breakLabel = new NodeLabel(world.intLabels);
        NodeLabel* oldContinueLabel = continueLabel; continueLabel = new NodeLabel(world.intLabels);
        NodeLabel* startLabel = new NodeLabel(world.intLabels);
        p.needToken("(");
        Node *aFirst=0, *aLast=0;
        if(!p.ifToken(";"))
        {
            do
            {
                Tree::linkNode(aFirst, aLast, readVar(-1));
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
        Node *cFirst=0, *cLast=0;
        if(!p.ifToken(")"))
        {
            do
            {
                Tree::linkNode(cFirst, cLast, readVar());
            }
            while(p.ifToken(","));
            p.needToken(")");
        }

        Node* cmd = readCommand();

        Tree::linkNode(aFirst, aLast, startLabel);
        if(!cFirst) Tree::linkNode(aFirst, aLast, continueLabel);
        if(cond) Tree::linkNode(aFirst, aLast, allocJmp(breakLabel, cond, true));
        Tree::linkNode(aFirst, aLast, cmd);
        if(cFirst)
        {
            Tree::linkNode(aFirst, aLast, continueLabel);
            Tree::linkNode(aFirst, aLast, cFirst);
            Tree::linkNode(aFirst, aLast, allocJmp(startLabel));
        }
        else
        {
            Tree::linkNode(aFirst, aLast, allocJmp(startLabel));
        }
        Tree::linkNode(aFirst, aLast, breakLabel);

        breakLabel    = oldBreakLabel;
        continueLabel = oldContinueLabel;
        return aFirst;
    }

  if(p.ifToken("if")) {
    p.needToken("(");
    NodeVar* cond = readVar();
    p.needToken(")");
    Node* t = readCommand();
    Node* f = 0;
    if(p.ifToken("else")) f = readCommand();

    // Оптимизация //! Условие 1==1
    if(cond->nodeType == ntConstI) {
      if(cond->cast<NodeConst>()->value) {
        delete cond;
        delete f;
        return t;
      } else {
        delete cond;
        delete t;
        return f;
      }
    }

    return allocNodeIf(cond, t, f);
  }

  if(p.ifToken("switch")) {
    NodeLabel* oldBreakLabel = breakLabel; breakLabel = new NodeLabel(world.intLabels);
    p.needToken("(");
    NodeVar* var = readVar();
    p.needToken(")");
    p.needToken("{");
    NodeSwitch* s = new NodeSwitch(var);
    NodeSwitch* saveSwitch = lastSwitch; lastSwitch = s;
    Node *first=0, *last=0;
    Tree::linkNode(first, last, readBlock());
    Tree::linkNode(first, last, breakLabel);
    s->body = first;
    if(!s->defaultLabel) s->defaultLabel = breakLabel;
    lastSwitch = saveSwitch;
    breakLabel = oldBreakLabel;
    return s;
  }

  if(lastSwitch && p.ifToken("default")) {
    p.needToken(":");
    Node* n = new NodeLabel(world.intLabels);
    lastSwitch->setDefault(n);
    return n;
  }

  if(lastSwitch && p.ifToken("case")) {
    int i = readConst(lastSwitch->var->dataType);
    p.needToken(":");
    Node* n = new NodeLabel(world.intLabels);
    lastSwitch->addCase(i, n);
    return n;
  }

  if(p.ifToken("return")) {
    if(!curFn->retType.isVoid()) return new NodeReturn(nodeConvert(readVar(), curFn->retType));
    return new NodeReturn(0);
  }

  bool reg = p.ifToken("register");
  Type t = readType(false);
  if(t.baseType != cbtError) {
    Node *first=0, *last=0;
    do {
      Type t1 = t;
      readModifiers(t1);
      const char* n = p.needIdent();
      unsigned stackOff = stackVars.size()==0 ? 10 : (stackVars.back().stackOff + stackVars.back().type.size());
      unsigned stackSize = (stackVars.back().stackOff + stackVars.back().type.size() + 1)&~1;
      if(curFn->stackSize < stackSize) curFn->stackSize = stackSize;
      if(t1.size()>=2) stackOff = (stackOff+1)&~1;
      stackVars.push_back(Var());
      Var& v = stackVars.back();
      v.name = n;
      if(p.ifToken("[")) {
        t1.arr = readConstU16(); //p.needInteger();
        if(t1.arr <= 0) throw std::runtime_error("[]");
        p.needToken("]");
        t1.addr++;
      }
      //const char* nn = stringBuf(n);
      v.stackOff = stackOff;
      v.type = t1;
      //!asm_decl(curFn->decl, fnName, n, t1);
//      Type t2 = t1;
//      t2.addr++;
//!!!!!!!!!!      NodeConst nc(fnName+"_"+n, t2, /*stack*/true);
//!!!!!!!!!!      curFn->localVars.push_back(nc.var);
      if(p.ifToken("=")) {
        //putRemark();
        Tree::linkNode(first, last, nodeOperator(oSet, getStackVar(v), nodeConvert(readVar(-1), t1)));
      }
    } while(p.ifToken(","));
    p.needToken(";");
    return first;
  }
  if(reg) p.syntaxError();

  // Команды
  Node *first=0, *last=0;

  do {
    Tree::linkNode(first, last, readVar(-1));
  } while(p.ifToken(","));
  p.needToken(";");
  return first;
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

Node* Parser::readCommand()
{
    std::string r;
    getRemark(r); //! Попадает только первая строка!
    Node* n = readCommand1();
    if(n) n->remark.swap(r);
    return n;
}

//---------------------------------------------------------------------------------------------------------------------

Node* Parser::readBlock() {
  Node *first = 0, *last = 0;
  int s = stackVars.size();
  while(!p.ifToken("}")) {
    Tree::linkNode(first, last, readCommand());
  }
  stackVars.resize(s);
  return first;
}

//---------------------------------------------------------------------------------------------------------------------

Function* Parser::parseFunction(Type& retType, const std::string& fnName)
{
    std::vector<Type> argTypes;
    if(!p.ifToken(")"))
    {
        int off = 0;
        do
        {
            if(p.ifToken("..."))
            {
                //! any
                break;
            }

            Type t = readType(true);
            readModifiers(t);
            if(t.baseType==cbtVoid && t.addr==0) break;

            stackVars.push_back(Var());
            Var& v = stackVars.back();

            if(p.ifToken(ttWord))
            {
                //nn = stringBuf(p.buf);
                //stackNamesI[p.loadedText] = stackNamesD.size()-1;
                v.name = p.loadedText;
                //argNames.push_back(p.buf);
            }
            else
            {
                //argNames.push_back("");
            }
            if(p.ifToken("["))
            {
                p.needToken("]");
                t.addr++;
            }
            argTypes.push_back(t);
            v.stackOff = off;
            v.type = t;

            off += v.type.size();
        } while(p.ifToken(","));
        p.needToken(")");
    }

    Function* f = world.findFunction(fnName);
    if(f == 0)
    {
        //functionNames.push_back(fnName);
        world.functions.push_back(Function());
        f = &world.functions.back();
        f->name = fnName;
        f->args = argTypes;
        f->retType = retType;
        f->stackSize = 0;
    }
    else
    {
        //!!!!!!!!!! Сравнить типы
    }

    //::fnName = fnName;
    //::retType = retType;

    /*
    if(p.ifToken("@")) {
        if(p.ifInteger(f->addr)) {
            p.needToken(";");
            f->callAddr = true;
            return;
        }
        f->needInclude = p.needString2();
        p.needToken(";");
        return;
    }
    */

    if(p.ifToken(";")) return 0; // proto

    p.needToken("{");

  //!int opt=0;
    //asm_preStartProc(fnName);

/*
  if(!retType.isVoid() && !retType.isStackType()) {
      asm_decl(f->decl, fnName, "0", retType);
  }

  for(unsigned int i=0; i<f->args.size(); i++) {
    //! if(i==f.argNames.size()-1 && f.args[i].isStackType()) break;
    asm_decl(f->decl, fnName, i2s(i+1), f->args[i]);
  }
*/
//  retPopBC = true/*opt!=0*/;
 // int cc = asm_startProc(fnName);

//  if(f.args.size() != 0) {
//    Type t = f.args.back();
//    if(t.isStackType() && stackNamesD[f.args.size()-1].name!=0) {
//      if(t.is8()) w.ld_ref_a (fnName+"_"+i2s(f.args.size()));
//             else w.ld_ref_hl(fnName+"_"+i2s(f.args.size()));
//    }
//  }


//  retPopBC = opt!=0;
//  asm_correctProc(cc, opt!=0);

  //returnLabel = labelesCnt++;

    curFn = f;
    f->rootNode = readBlock();
    curFn = 0;

    if(!f->rootNode)
    {
        f->rootNode = new NodeReturn(0);
    }
    else
    {
        Node* n = f->rootNode;
        while(n->next) n = n->next;
        if(n->nodeType != ntReturn)
        {
            n->next = new NodeReturn(0);
        }
    }

    return f;


  //asm_label(returnLabel);

//  asm_endProc(retPopBC);
}

//---------------------------------------------------------------------------------------------------------------------

void Parser::arrayInit(std::vector<char>& data, Type& fnType)
{
  // Эта функция не генерирует код
  NodeVar* c = readVar(-1);

  if(fnType.size()==1) { //|| (fnType.addr==1 && (fnType.baseType==cbtChar || fnType.baseType==cbtUChar))
    if(c->nodeType == ntConstI) {
      data.push_back(c->cast<NodeConst>()->value & 0xFF);
      delete c;
      return;
    }
    throw std::runtime_error("not imp");
  }

  if(fnType.addr) { //|| (fnType.addr==1 && (fnType.baseType==cbtChar || fnType.baseType==cbtUChar))
    if(c->nodeType == ntConstI) {
      int v = c->cast<NodeConst>()->value;
      data.push_back(v & 0xFF);
      data.push_back((v >> 8) & 0xFF);
      delete c;
      return;
    }
    throw std::runtime_error("not imp");
  }

  /*
  if(fnType.size()==2) {
//    init2.str(" .dw ");
    mask = 0xFFFF;
  } else {
    p.logicError_("Только 8 и 16 бит");
  }
  */

  throw std::runtime_error("not imp");
}

//---------------------------------------------------------------------------------------------------------------------

static const char* operators1[] = {
  ".", "..", "...", "++", "--", "/", "%", "*", "$", "+", "-",
  "<<", ">>", "<", ">", "<=", ">=", "==", "!=", "&",  "^",  "|", "&&",
  "||", "?", "=", "+=", "-=", "*=", "/=", "%=", ">>=", "<<=", "&=", "^=", "|=",
  "->", "//", "/*", "//", 0
};

//---------------------------------------------------------------------------------------------------------------------

void Parser::parse2()
{
    //stackNamesI.clear();
    stackVars.resize(0);
    //stackTypes.resize(0);

    while(p.token!=ttOperator || p.tokenText[0]!='}')
    {
        bool typedef1 = p.ifToken("typedef");
        bool extren1 = !typedef1 && p.ifToken("extern");
        bool static1 = !typedef1 && !extren1 && p.ifToken("static");
        Type fnType1 = readType(true);
        if(fnType1.baseType == cbtError) p.syntaxError();

        //!!!!if(p.ifToken(";")) continue;

        while(1)
        {
            Type fnType = fnType1;
            readModifiers(fnType);

            std::string fnName = p.needIdent();

            if(p.ifToken("(")) {
                if(typedef1) p.syntaxError("typedef не поддерживает функции");
                parseFunction(fnType, fnName);
                break;
            }

            if(fnType.arr==0 && p.ifToken("[")) {
                if(p.ifToken("]")) {
                    fnType.arr = 1;
                } else {
                    fnType.arr = readConstU16();
                    if(fnType.arr <= 0) throw std::runtime_error("[]");
                    p.needToken("]");
                }
                fnType.addr++;
                //fnType.place = pConst;
                if(p.ifToken("[")) {
                    fnType.addr++;
                    fnType.arr2 = readConstU16();
                    if(fnType.arr2 <= 0) throw std::runtime_error("[]");
                    p.needToken("]");
                    if(p.ifToken("[")) {
                        fnType.addr++;
                        fnType.arr3 = readConstU16();
                        if(fnType.arr3 <= 0) throw std::runtime_error("[]");
                        p.needToken("]");
                    }
                }
            }

            if(typedef1) {
                world.typedefs.push_back(Typedef());
                Typedef& t = world.typedefs.back();
                t.name = fnName;
                t.type = fnType;
            } else {
                if(!extren1) {
                    std::vector<char> data;
                    //bool ptr = fnType.arr==0 && fnType.addr;
                    if(p.ifToken("=")) {
                        if(p.ifToken(ttString2)) {
                            int ptr = 0;
                            do {
                                std::string s = p.loadedText; //! Убрать
                                data.resize(ptr+s.size()+1);
                                memcpy(&data[ptr], s.c_str(), s.size()+1);
                                ptr += s.size();
                            } while(p.ifToken(ttString2));
                        } else
                        if(fnType.addr>0 && p.ifToken("{")) {
                            Type fnType1 = fnType;
                            fnType1.addr--;
                            fnType1.arr=0;
                            while(1) {
                                if(p.ifToken("}")) break;
                                if(fnType1.addr==0 && fnType1.arr==0 && fnType1.baseType==cbtStruct) {
                                    if(p.ifToken("{")) {
                                        if(fnType1.s == 0) throw std::runtime_error("s==0");
                                        Struct& s = *fnType1.s;
                                        for(unsigned int u=0; u<s.items.size(); u++) {
                                            if(u>0) p.needToken(",");
                                            arrayInit(data, s.items[u].type);
                                        }
                                        p.needToken("}");
                                    }
                                } else {
                                    arrayInit(data, fnType1);
                                }
                                if(p.ifToken("}")) break;
                                p.needToken(",");
                            }
                        } else {
                            if(fnType.arr) p.syntaxError("Нельзя задавать адрес массива");
                            arrayInit(data, fnType);
                //              ptr = false;
                        }
                    }

                    /*if(fnType.arr2!=0) {
                    // Указатель на данные
                    fnType.addr--;
                    //asm_decl3(outDecl, fnName, fnType.size()*fnType.arr2 - data.size(), &data, fnType.arr, fnType.size());
                    fnType.addr++;
                    } else {
                    //asm_decl3(outDecl, fnName, fnType.size() - data.size(), &data, fnType.arr==0 && fnType.addr!=0);
                    }*/
                } else {
              /*if(p.ifToken("@")) {
                  if(p.ifInteger(f.addr)) {
                  p.needToken(";");
                  f.callAddr = true;
                  return;
                }
                fnType.needInclude = p.needString2();
              }*/
                }
                world.globalVars.push_back(fnName);
                world.globalVarsT.push_back(fnType);
            }

            if(p.ifToken(";")) break;
            p.needToken(",");
        }
    }
}

void Parser::parse(unsigned step)
{
    bool old_cfg_eol = p.cfg_eol; p.cfg_eol = false;
    const char** old_cfg_operators = p.cfg_operators; p.cfg_operators = &operators1[0];
    static const char* cRem[] = { "//", 0 };
    const char** old_cfg_remark = p.cfg_remark; p.cfg_remark = cRem;
    p.cfg_remark = cRem;
    p.cfg_caseSel = true;
    bool old__decimalnumbers = p.cfg_decimalnumbers; p.cfg_decimalnumbers = true;
    p.needToken(ttEol);

    if(step==0)
    {
        parse2();
    }
    else
    {
        unsigned l=0;
        for(;;)
        {
            if(l==0 && p.token==ttOperator && p.tokenText[0]=='}') break;
            if(p.ifToken("}")) { l--; continue; }
            if(p.ifToken("{")) { l++; continue; }
            p.nextToken();
        }
    }

    p.cfg_caseSel = false;
    p.cfg_eol = old_cfg_eol;
    p.cfg_operators = old_cfg_operators;
    p.cfg_remark = old_cfg_remark;
    p.cfg_decimalnumbers = old__decimalnumbers;
    p.nextToken();
}

}
