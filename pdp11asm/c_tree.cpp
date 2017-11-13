// PDP11 Assembler (c) 08-01-2017 ALeksey Morozov (aleksey.f.morozov@gmail.com)

#include "c_tree.h"
#include <string.h>
#include "tools.h"

namespace C
{

class TreePrepare {
public:
    unsigned prepare_arg;
    unsigned labelsCnt;

    TreePrepare() { prepare_arg=0; labelsCnt=0; }
    void prepare(Node* n);
};

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
    if(addr<1) return 1;
    addr-=1;
    int s = sizeAsPtr();
    addr+=1;
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

std::string Tree::regString(const std::string& str) //! Переписать на массив
{
    // Строка уже добавлена?
    std::map<std::string, int>::iterator i = strs.find(str);
    if(i != strs.end())
    {
        // Возвращаем её имя
        char name[16];
        snprintf(name, sizeof(name), "$%u", i->second);
        return name;
    }

    // Индекс
    char name[16];
    snprintf(name, sizeof(name), "$%u", strsCounter);
    strs[str] = strsCounter++;

    // Строка - это глобальная переменная
    globalVars.push_back(GlobalVar());
    GlobalVar& g = globalVars.back();
    g.name          = name;
    g.type.baseType = cbtChar;
    g.type.addr     = 1;
    g.type.arr      = 1;
    size_t s1 = str.size()+1;
    if(s1==1)
    {
        g.data.push_back(0);
    }
    else
    {
        g.data.resize(s1);
        memcpy(&g.data[0], str.c_str(), s1);
        g.z = true;
    }

    return name;
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

Block::Block()
{
    first = 0;
    last = 0;
}

//---------------------------------------------------------------------------------------------------------------------

void Block::operator << (Node* element)
{
    if(element)
    {
        element = Tree::postIncOpt(element);
        if(first == 0)
        {
           first = last = element;
        }
        else
        {
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

void treePrepare(Function* f)
{
    TreePrepare p;
    p.prepare_arg = f->stackSize;
    p.prepare(f->root);
    f->labelsCnt = p.labelsCnt;
}

//---------------------------------------------------------------------------------------------------------------------

void TreePrepare::prepare(Node* n)
{
    for(;n; n=n->next)
    {
        switch(n->nodeType)
        {
            case ntConst:
            {
                NodeConst* c = n->cast<NodeConst>();
                if(c->prepare)
                {
                    c->value += prepare_arg;
                    c->prepare = false;
                }
                break;
            }
            case ntConvert:
                prepare(n->cast<NodeConvert>()->var);
                break;
            case ntCall:
            {
                NodeCall* c = n->cast<NodeCall>();
                for(unsigned i=0; i<c->args1.size(); i++)
                    prepare(c->args1[i]);
                break;
            }
            case ntDeaddr:
                prepare(n->cast<NodeDeaddr>()->var);
                break;
            case ntSwitch:
                prepare(n->cast<NodeSwitch>()->var);
                prepare(n->cast<NodeSwitch>()->body);
                break;
            case ntReturn:
                prepare(n->cast<NodeReturn>()->var);
                break;
            case ntMonoOperator:
                prepare(n->cast<NodeMonoOperator>()->a);
                break;
            case ntOperator:
                prepare(n->cast<NodeOperator>()->a);
                prepare(n->cast<NodeOperator>()->b);
                break;
            case ntOperatorIf:
                prepare(n->cast<NodeOperatorIf>()->a);
                prepare(n->cast<NodeOperatorIf>()->b);
                prepare(n->cast<NodeOperatorIf>()->cond);
                break;
            case ntJmp:
                prepare(n->cast<NodeJmp>()->cond);
                break;
            case ntIf:
                prepare(n->cast<NodeIf>()->cond);
                prepare(n->cast<NodeIf>()->t);
                prepare(n->cast<NodeIf>()->f);
                break;
            case ntLabel:
                n->cast<NodeLabel>()->n1 = labelsCnt++;
                break;
            case ntAsm:
            case ntSP:
                break;
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------

Node* Tree::allocIf(NodeVar* cond, Node* t, Node* f)
{
    // Константа в условии
    if(cond->isConst())
    {
        if(!cond->cast<NodeConst>()->isNull())
        {
            delete cond;
            delete f;
            return t;
        }
        else
        {
            delete cond;
            delete t;
            return f;
        }
    }

    return outOfMemory(new NodeIf(cond, t, f));
}

//---------------------------------------------------------------------------------------------------------------------

NodeVar* Tree::allocOperatorIf(NodeVar* cond, NodeVar* a, NodeVar* b)
{
    assert(a->dataType.eq(b->dataType));

    // Константа в условии
    if(cond->isConst())
    {
        if(!cond->cast<NodeConst>()->isNull())
        {
            delete cond;
            delete b;
            return a;
        }
        else
        {
            delete cond;
            delete a;
            return b;
        }
    }

    return outOfMemory(new NodeOperatorIf(a->dataType, a, b, cond));
}

//---------------------------------------------------------------------------------------------------------------------

NodeJmp* Tree::allocJmp(NodeLabel* label, NodeVar* cond, bool ifFalse)
{
    // Константа в условии
    if(cond && cond->isConst())
    {
        bool constFalse = cond->cast<NodeConst>()->isNull();
        if(ifFalse) constFalse = !constFalse;
        delete cond;
        cond = 0;
        if(constFalse) return 0;
    }

    return outOfMemory(new NodeJmp(label, cond, ifFalse));
}

bool Node::isConstI()
{
    return nodeType==ntConst && cast<NodeConst>()->text.empty();
}

bool Node::isDeaddrConst()
{
    return nodeType==ntDeaddr && cast<NodeDeaddr>()->var->nodeType==ntConst;
}

}
