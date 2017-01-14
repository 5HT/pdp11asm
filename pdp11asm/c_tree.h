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

namespace C
{
    enum BaseType { cbtError, cbtVoid, cbtStruct, cbtChar, cbtShort, cbtLong, cbtUChar, cbtUShort, cbtULong };

    struct Struct;

    class Type {
    public:
        BaseType baseType;
        int addr, arr, arr2, arr3;
        Struct* s;

        bool operator == (Type a) const
        {
            return baseType==a.baseType && addr==a.addr && s==a.s && arr==a.arr && arr2==a.arr2 && arr3==a.arr3;
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

    class NodeSwitch : public Node {
    public:
        NodeVar* var;
        Node* body;
        Node* defaultLabel;
        std::map<unsigned int, Node*> cases;

        NodeSwitch(NodeVar* _var)
        {
            nodeType     = ntSwitch;
            body         = 0;
            var          = _var;
            defaultLabel = 0;
        }

      void setDefault(Node* label)
      {
          defaultLabel = label;
      }

      void addCase(int value, Node* label)
      {
          if(cases.find(value) != cases.end()) throw std::runtime_error("NodeSwitch.addCase");
          cases[value] = label;
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
        int32_t value;
        bool    prepare;
        std::string text;

        NodeConst(const std::string& _text, Type _dataType) {
            nodeType = ntConstS;
            dataType = _dataType;
            text     = _text;
            value    = 0;
            prepare  = false;
        }

        NodeConst(int32_t _value, Type _dataType = cbtUShort)
        {
            nodeType = ntConstI;
            dataType = _dataType;
            value    = _value;
            prepare  = false;
        }

        bool isNull() { return value==0 && text.empty(); }
    };

    class NodeCall : public NodeVar {
    public:
        std::vector<NodeVar*> args;
        int addr;
        std::string name;

        NodeCall(int _addr, const std::string& _name, Type _dataType) { nodeType = ntCall; addr = _addr; name = _name; dataType = _dataType; }
        ~NodeCall() { while(!args.empty()) { delete args.back(); args.pop_back(); } }
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

    class NodeLabel : public Node {
    public:
        unsigned n;
        NodeLabel(unsigned& intLabels)
        {
            nodeType = ntLabel;
            n = intLabels++;
        }
        ~NodeLabel();
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

        NodeJmp() {
            nodeType = ntJmp;
            label = 0;
            cond = 0;
            ifZero = false;
        }
    };

    struct Function {
        Type              retType;
        std::string       name;
        std::vector<Type> args;
        std::string       needInclude;
        int               addr;
        unsigned          stackSize;
        Node*             rootNode;
        bool              compiled;

        Function() { compiled=false; rootNode=0; addr=0; stackSize=0; }
        ~Function() { delete rootNode; }
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

        GlobalVar() { extren1=false; z=false; compiled=false; }
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
        unsigned                   intLabels;
        int                        strsCounter;
        std::map<std::string, int> strs;

        Tree()
        {
            userStructCnt = 0;
            strsCounter = 1;
            intLabels = 0;
        }

        std::string regString(const std::string& str);
        Function* findFunction(const std::string& name);
        bool checkUnique(const std::string& name) { return unique.find(name) == unique.end(); }
        static Node* postIncOpt(Node* v);
        static void linkNode(Node*& first, Node*& last, Node* element);
        static Operator inverseOp(Operator o);

        Function*  addFunction (const char* name) { functions.push_back(Function()); Function* f=&functions.back(); f->name=name; unique[name]=1; return f; }
        GlobalVar* addGlobalVar(const char* name) { globalVars.push_back(GlobalVar()); GlobalVar* v=&globalVars.back(); v->name=name; unique[name]=2; return v; }
        Typedef*   addTypedef  (const char* name, const Type& type) { typedefs.push_back(Typedef()); Typedef* t=&typedefs.back(); t->name=name; t->type=type; unique[name]=3; return t; }

        unsigned prepare_arg;
        void prepare(Node* n);
    };
}
