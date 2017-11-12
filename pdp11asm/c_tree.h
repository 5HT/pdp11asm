// PDP11 Assembler (c) 08-01-2017 ALeksey Morozov (aleksey.f.morozov@gmail.com)

#pragma once

#include <map>
#include <assert.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <stdio.h>
#include <stdint.h>
#include <list>
#include "parser.h"

namespace C
{
    enum BaseType {
        cbtError,
        cbtVoid,
        cbtStruct,
        cbtChar,
        cbtShort,
        cbtLong,
        cbtUChar,
        cbtUShort,
        cbtULong
    };

    struct Struct;

    class Type {
    public:
        BaseType baseType;
        int addr, arr, arr2, arr3;
        Struct* s;

        bool eq(const Type& a) const
        {
            if(baseType==cbtStruct && s != a.s) return false;
            return baseType==a.baseType && addr==a.addr;
        }

        inline Type(BaseType _baseType = cbtError) { arr=arr2=arr3=addr=0; s=0; baseType=_baseType; }

        //std::string descr();

        unsigned size() { return arr ? arr*sizeElement() : sizeAsPtr(); } // Массив занимет заданный объем памяти
        unsigned sizeAsPtr(); // Массив имеет размер указателя
        unsigned sizeElement(); // Размер элемента массива
        unsigned sizeForInc(); // Размер для ++, --

        inline bool isVoid()   { return baseType==cbtVoid && addr==0; }
        inline bool is8()      { return (baseType==cbtChar || baseType==cbtUChar) && addr==0; }
        inline bool is16()     { return baseType==cbtUShort || baseType==cbtShort || addr!=0; }
        inline bool is8_16()   { return (baseType==cbtUShort || baseType==cbtShort || baseType==cbtChar || baseType==cbtUChar) || addr!=0; }
        inline bool is32()     { return (baseType==cbtULong || baseType==cbtLong) && addr==0; }
        inline bool isSigned() { return (baseType==cbtChar || baseType==cbtShort || baseType==cbtLong) && addr==0; }

        inline char b()
        {
            char pf;
            if(is8()) return 'B';
            if(is16()) return 'W';
            throw std::runtime_error("int");
        }

    /*
        unsigned getSize()
        {
            if(is8()) return 8;
            if(is16()) return 16;
            if(is32()) return 32;
            return 0;
        }
    */
    };

    enum NodeType {
      ntConstI, ntConstS, ntConvert, ntCall, ntDeaddr, ntSwitch, ntLabel,
      ntReturn, ntMonoOperator, ntOperator, ntOperatorIf, ntJmp, ntIf, ntAsm, ntSP
    };

    enum Reg {
      regNone=0, regA=1, regHL=2, regDE=4, regBC=8, regB=16, regC=32, regD=64, regE=128, regH=256, regL=512
    };

    enum MonoOperator {
      moNot, moNeg, moAddr, moDeaddr, moPostInc, moPostDec, moInc, moDec, moXor, moIncVoid, moDecVoid
    };

    enum Operator {
      oNone, oDiv, oMod, oMul, oAdd, oSub, oShl, oShr, oL, oG, oLE, oGE, oE, oNE, oAnd, oXor,
      oOr, oLAnd, oLOr, oIf, oSet, oSetVoid, oSAdd, oSSub, oSMul, oSDiv, oSMod, oSShl, oSShr, oSAnd, oSXor, oSOr, oCount
    };

    struct StructItem {
        std::string name;
        Type type;
        int offset;
    };

    struct Struct {
        std::string name;
        std::vector<StructItem> items;
        int size;
    };

    class Node {
    public:
        NodeType nodeType;
        Node* next;
        std::string remark;

        Node() { next=0; }
        ~Node() { delete next; }
        template<class T> T* cast() { return (T*)this; }
        bool isConst() { return nodeType==ntConstI || nodeType==ntConstS; }
    };

    class NodeVar : public Node {
    public:
        Type dataType;

        Reg isRegVar();
    };

    class NodeLabel : public Node {
    public:
        unsigned n1;
        NodeLabel()
        {
            nodeType = ntLabel;
            n1 = 0;
        }
        ~NodeLabel();
    };

    class NodeSwitch : public Node {
    public:
        NodeVar* var;
        Node* body;
        NodeLabel* defaultLabel;
        std::map<unsigned int, NodeLabel*> cases;

        NodeSwitch(NodeVar* _var)
        {
            nodeType     = ntSwitch;
            body         = 0;
            var          = _var;
            defaultLabel = 0;
        }

        bool setDefault(NodeLabel* label)
        {
            if(defaultLabel) return false;
            defaultLabel = label;
            return true;
        }

        bool addCase(int value, NodeLabel* label)
        {
            if(cases.find(value) != cases.end()) return false;
            cases[value] = label;
            return true;
        }
    };

    class NodeConvert : public NodeVar {
    public:
        NodeVar* var;

        NodeConvert(NodeVar* _var, Type _dataType)
        {
            nodeType = ntConvert;
            var      = _var;
            dataType = _dataType;
        }
    };

    class NodeReturn : public Node {
    public:
        NodeVar* var;

        NodeReturn(NodeVar* _var)
        {
            nodeType = ntReturn;
            var = _var;
        }
    };

    class NodeAsm : public Node {
    public:
        std::string text;
        NodeAsm(std::string _text)
        {
            nodeType = ntAsm;
            text = _text;
        }
    };

    class NodeSP : public NodeVar {
    public:
        NodeSP() {
            nodeType = ntSP;
            dataType = cbtUShort;
        }
    };

    class NodeConst : public NodeVar {
    public:
        uint32_t    value;
        bool        prepare;
        std::string text;

        NodeConst(const std::string& _text, Type _dataType) {
            nodeType = ntConstS;
            dataType = _dataType;
            text     = _text;
            value    = 0;
            prepare  = false;
        }

        NodeConst(uint32_t _value, Type _dataType)
        {
            nodeType = ntConstI;
            dataType = _dataType;
            value    = _value;
            prepare  = false;
        }

        bool isNull() { return value==0 && text.empty(); }
    };

    class FunctionArg {
    public:
        unsigned          n;
        Type              type;
        unsigned          reg;

        FunctionArg(unsigned _n, Type _type, unsigned _reg) : n(_n), type(_type), reg(_reg) {};
    };

    struct Function {
        Type                     retType;
        std::string              name;
        std::vector<FunctionArg> args;
        std::string              needInclude;
        int                      addr;
        unsigned                 stackSize;
        Node*                    root;
        bool                     parsed;
        bool                     compiled;
        unsigned                 labelsCnt;
        unsigned                 call_type;
        unsigned                 call_arg;
        unsigned                 reg;

        Function() { compiled=false; parsed=false; root=0; addr=0; stackSize=0; labelsCnt=0; call_type=0; reg=0; }
        ~Function() { delete root; }
    };

    class NodeCall : public NodeVar {
    public:
        std::vector<NodeVar*> args1;
        Function* f;
        int addr;
        std::string name;

        NodeCall(int _addr, const std::string& _name, Type _dataType, Function* _f) { nodeType = ntCall; addr = _addr; name = _name; dataType = _dataType; f = _f; }
        ~NodeCall() { while(!args1.empty()) { delete args1.back(); args1.pop_back(); } }
    };

    class NodeMonoOperator : public NodeVar {
    public:
        MonoOperator o;
        NodeVar* a;

        NodeMonoOperator(NodeVar* _a, MonoOperator _o) { nodeType = ntMonoOperator; a =_a; o = _o; dataType = a->dataType; }
        ~NodeMonoOperator() { delete a; }
    };

    class NodeOperator : public NodeVar {
    public:
        Operator o;
        NodeVar* a;
        NodeVar* b;

        NodeOperator(const Type& _dataType, Operator _o, NodeVar* _a, NodeVar* _b) { nodeType = ntOperator; dataType = _dataType; o=_o; a=_a; b=_b; }
        ~NodeOperator() { delete a; delete b; }
    };

    class NodeOperatorIf : public NodeVar {
    public:
        NodeVar* a;
        NodeVar* b;
        NodeVar* cond;

        NodeOperatorIf(const Type& _dataType, NodeVar* _a, NodeVar* _b, NodeVar* _cond) { nodeType = ntOperatorIf; dataType = _dataType; a=_a; b=_b; cond=_cond; }
        ~NodeOperatorIf() { delete a; delete b; delete cond; }
    };


    class NodeDeaddr : public NodeVar {
    public:
        NodeVar* var;

        NodeDeaddr(NodeVar* _var)
        {
            nodeType = ntDeaddr;
            var = _var;
            if(var->dataType.addr==0) throw std::runtime_error("NodeDeaddr");
            dataType = var->dataType;
            dataType.addr--;
        }

        NodeDeaddr(NodeVar* _var, Type t) {
            nodeType = ntDeaddr;
            var = _var;
            dataType = t;
        }
    };

    class NodeIf : public Node {
    public:
        NodeVar* cond;
        Node* t;
        Node* f;

        NodeIf(NodeVar* _cond, Node* _t, Node* _f) { nodeType=ntIf; cond=_cond; t=_t; f=_f; }
        ~NodeIf() { delete cond; delete t; delete f; }
    };    

    class NodeJmp : public Node {
    public:
        NodeLabel* label;
        NodeVar* cond;
        bool  ifZero;

        NodeJmp(NodeLabel* _label, NodeVar* _cond=0, bool _ifZero=false) { nodeType=ntJmp; label=_label; cond=_cond; ifZero=_ifZero; }
        ~NodeJmp() { delete cond; }
    };

    struct Typedef {
        std::string name;
        Type type;
    };

    class GlobalVar {
    public:
        std::string          name;
        Type                 type;
        bool                 extren1;
        std::vector<uint8_t> data;
        bool                 compiled;
        bool                 z;
        bool                 reg;

        GlobalVar() { extren1=false; z=false; compiled=false; reg=false; }
    };

    class Tree
    {
    public:
        typedef std::map<std::string, unsigned> Unique;
        std::list<Typedef>   typedefs;
        std::list<Function>  functions;
        std::list<GlobalVar> globalVars;
        Unique               unique;
        std::list<Struct>    structs;

        unsigned                   userStructCnt;
        //unsigned                   labelsCnt;
        int                        strsCounter;
        std::map<std::string, int> strs;

        Tree()
        {
            userStructCnt = 0;
            strsCounter = 1;
        }

        std::string regString(const std::string& str);
        Function* findFunction(const std::string& name);
        bool checkUnique(const std::string& name) { return unique.find(name) == unique.end(); }
        static Node* postIncOpt(Node* v);
        static Operator inverseOp(Operator o);

        Function*  addFunction (const char* name) { functions.push_back(Function()); Function* f=&functions.back(); f->name=name; unique[name]=1; return f; }
        GlobalVar* addGlobalVar(const char* name) { globalVars.push_back(GlobalVar()); GlobalVar* v=&globalVars.back(); v->name=name; unique[name]=2; return v; }
        Typedef*   addTypedef  (const char* name, const Type& type) { typedefs.push_back(Typedef()); Typedef* t=&typedefs.back(); t->name=name; t->type=type; unique[name]=3; return t; }

        static Node*    allocIf(NodeVar* cond, Node* t, Node* f);
        static NodeVar* allocOperatorIf(NodeVar* cond, NodeVar* t, NodeVar* f);
        static NodeJmp* allocJmp(NodeLabel* label, NodeVar* cond=0, bool ifFalse=false);
    };

    class Block {
    public:
        Node* first;
        Node* last;

        Block();
        void operator << (Node* element);
    };

    void treePrepare(Function* f);
}

