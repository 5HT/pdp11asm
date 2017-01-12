// PDP11 Assembler (c) 08-01-2017 ALeksey Morozov (aleksey.f.morozov@gmail.com)

#include "c_tree.h"

namespace C
{

//---------------------------------------------------------------------------------------------------------------------

unsigned Type::sizeElement()
{
    if(addr==0) return sizeAsPtr();
    addr--;
    int s = sizeAsPtr();
    addr++;
    return s;
}

//---------------------------------------------------------------------------------------------------------------------

unsigned Type::sizeForInc()
{
    if(addr==0) return 1;
    addr--;
    int s = sizeAsPtr();
    addr++;
    return s;
}

//---------------------------------------------------------------------------------------------------------------------

unsigned Type::sizeAsPtr() {
    if(addr > 0) return 2;
    switch(baseType) {
        case cbtVoid:   return 0;
        case cbtUChar:  case cbtChar:  return 1;
        case cbtUShort: case cbtShort: return 2;
        case cbtULong:  case cbtLong:  return 4;
        case cbtStruct: return s==0 ? 0 : s->size;
        default: throw std::runtime_error("CType.sizeAsPtr");
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Поместить в стек строку

std::string Tree::regString(const char* str)
{
    char buf[256];
    std::map<std::string, int>::iterator i = strs.find(str);
    if(i != strs.end())
    {
        snprintf(buf, sizeof(buf), "str%u", i->second);
    }
    else
    {
        snprintf(buf, sizeof(buf), "str%u", strsCounter);
        strs[str] = strsCounter++;
    }
    return buf;
}

//---------------------------------------------------------------------------------------------------------------------
// Найти функцию

Function* Tree::findFunction(const std::string& name)
{
    for(std::list<Function>::iterator f=functions.begin(); f!=functions.end(); f++)
        if(f->name == name)
            return &*f;
    return 0;
}

//---------------------------------------------------------------------------------------------------------------------

Node* Tree::postIncOpt(Node* v)
{
    // В deaddr нет смысла
    while(v->nodeType == ntDeaddr)
    {
        NodeDeaddr* vd = v->cast<NodeDeaddr>();
        NodeVar* b = vd->var;
        vd->var = 0;
        delete vd;
        v = b;
    }

    // Оптимизация
    if(v->nodeType == ntMonoOperator)
    {
        NodeMonoOperator* no = v->cast<NodeMonoOperator>();
        switch((unsigned)no->o)
        {
            case moInc: case moPostInc: no->o=moIncVoid; break;
            case moDec: case moPostDec: no->o=moDecVoid; break;
        }
        return v;
    }

    // PostInc и PostDec могут преобразовываться в A+константа и A-константа.
    if(v->nodeType == ntOperator) //! Проверить
    {
        NodeOperator* no = (NodeOperator*)v;
        if(no->b->isConst())
        {
            NodeVar* tmp;
            switch((unsigned)no->o)
            {
                case oAdd: tmp = no->a; no->a=0; delete no; return tmp;
                case oSub: tmp = no->a; no->a=0; delete no; return tmp;
            }
        }
        // oSet может быть проще
        if(no->o==oSet)
        {
            no->o = oSetVoid;
        }
    }
    return v;
}

//---------------------------------------------------------------------------------------------------------------------

void Tree::linkNode(Node*& first, Node*& last, Node* element)
{
    if(element)
    {
        element = postIncOpt(element);
        if(first == 0) {
           first = last = element;
        } else {
            while(last->next) last = last->next;
            last->next = element;
        }
        while(last->next) last = last->next;
    }
}

//---------------------------------------------------------------------------------------------------------------------

Operator Tree::inverseOp(Operator o)
{
    switch(o) {
        case oL:  return oGE;
        case oLE: return oG;
        case oG:  return oLE;
        case oGE: return oL;
        case oE:  return oNE;
        case oNE: return oE;
        default: throw std::runtime_error("inverseOp");
    }
}

//---------------------------------------------------------------------------------------------------------------------

}
