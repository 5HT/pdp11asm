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
      ntConstI, ntConstS, ntConvert, ntCallI, ntCallS, ntDeaddr, ntSwitch, ntLabel,
      ntReturn, ntMonoOperator, ntOperator, ntJmp, ntIf, ntAsm, ntSP
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
        std::string text;

        NodeConst(const std::string& _text, Type _dataType) {
            nodeType = ntConstS;
            dataType = _dataType;
            text     = _text;
            value    = 0;
        }

        NodeConst(int32_t _value, Type _dataType = cbtUShort)
        {
            nodeType = ntConstI;
            dataType = _dataType;
            value    = _value;
        }

        std::string name()
        {
            if(nodeType == ntConstI) {
                char buf[256];
                snprintf(buf, sizeof(buf), "%i", value);
                return buf;
            }
            if(nodeType == ntConstS) {
                assert(!text.empty());
                return text;
            }
            throw;
        }
    };

    class NodeCall : public NodeVar {
    public:
      std::vector<NodeVar*> args;
      int addr;
      std::string name;

      NodeCall(int _addr, Type _dataType, std::vector<NodeVar*>& _args) { nodeType = ntCallI; addr = _addr; dataType = _dataType; args=_args; }
      NodeCall(const std::string& _name, Type _dataType, std::vector<NodeVar*>& _args) { nodeType = ntCallS; name = _name; dataType = _dataType; args=_args; }
    };

    class NodeMonoOperator : public NodeVar {
    public:
        MonoOperator o;
        NodeVar* a;

        NodeMonoOperator(NodeVar* _a, MonoOperator _o) {
            nodeType = ntMonoOperator;
            a        =_a;
            o        = _o;
            dataType = a->dataType;
        }
    };

    class NodeOperator : public NodeVar {
    public:
        Operator o;
        NodeVar* a;
        NodeVar* b;
        NodeVar* cond;

        NodeOperator(const Type& _dataType, Operator _o, NodeVar* _a, NodeVar* _b, NodeVar* _cond)
        {
            dataType = _dataType; o=_o; a=_a; b=_b; cond=_cond;
            nodeType = ntOperator;
        }
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

        NodeIf() {
            nodeType = ntIf;
            cond = 0;
            t = 0;
            f = 0;
        }
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
        Type retType;
        std::string name;
        std::vector<Type> args;
        std::string needInclude;
        bool callAddr;
        int addr;
        unsigned stackSize;
        Node* rootNode;
        bool compiled;
        //std::vector<NodeVariable*> localVars;

        Function() { compiled=false; rootNode=0; callAddr=false; }
        ~Function() { delete rootNode; }
    };

    struct Typedef {
        std::string name;
        Type type;
    };

    class Tree
    {
    public:
        std::vector<std::string>   globalVars;
        std::vector<Type>          globalVarsT;
        std::list<Typedef>         typedefs;
        std::list<Struct>          structs;
        unsigned                   userStructCnt;
        unsigned                   intLabels;
        std::list<Function>        functions;
        Function*                  curFn;
        int                        strsCounter;
        std::map<std::string, int> strs;

        Tree()
        {
            userStructCnt = 0;
            curFn = 0;
            strsCounter = 1;
            intLabels = 0;
        }

        std::string regString(const char* str);
        Function* findFunction(const std::string& name);

        static Node* postIncOpt(Node* v);
        static void linkNode(Node*& first, Node*& last, Node* element);
        static Operator inverseOp(Operator o);
    };
}
