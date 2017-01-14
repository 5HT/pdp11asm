// PDP11 Assembler (c) 08-01-2017 ALeksey Morozov (aleksey.f.morozov@gmail.com)

#pragma once

#include "parser.h"
#include "c_tree.h"

namespace C
{
    class Parser {
    private:
        ::Parser& p;
        Tree& world;

        struct StackVar
        {
            Type        type;
            std::string name;
            unsigned    addr;
            bool        arg;
        };

        NodeLabel* breakLabel, *continueLabel;
        NodeSwitch* lastSwitch;
        std::vector<StackVar> stackVars;
        Function* curFn;

        bool p_ifToken(const std::vector<std::string>& strs, int& o);
        StackVar* p_ifToken(std::vector<StackVar>& a);
        Function* p_ifToken(std::list<Function>& a);
        GlobalVar* p_ifToken(std::list<GlobalVar>& a);
        Operator findOperator(int level, int& l);
        uint32_t readConst(Type& to);
        uint16_t readConstU16();

        void parseStruct(Struct& s, int m);
        NodeJmp* allocJmp(NodeLabel* _label, NodeVar* _cond, bool _ifZero);
        NodeJmp* allocJmp(NodeLabel* _label);
        NodeVar* nodeMonoOperator(NodeVar* a, MonoOperator o);
        NodeVar* nodeAddr(NodeVar* x);
        NodeVar* getStackVar(StackVar& x);
        NodeVar* bindVar_2();
        Type     readType(bool error);
        Struct* parseStruct3(int m);
        NodeVar* readVar(int level=-1);
        void     readModifiers(Type& t);
        NodeVar* bindVar();
        NodeVar* nodeConvert(NodeVar* x, Type type);
        NodeVar* nodeOperator(Operator o, NodeVar* a, NodeVar* b, bool noMul=false, NodeVar* cond=0);
        Node*    readBlock();
        Node*    readCommand();
        NodeVar* addFlag(NodeVar* a);
        NodeVar* nodeOperator2(Type type, Operator o, NodeVar* a, NodeVar* b);
        NodeVar* nodeOperatorIf(Type type, NodeVar* a, NodeVar* b, NodeVar* cond);
        Node*    readCommand1();
        void     getRemark(std::string& out);
        Function*parseFunction(Type& retType, const std::string& fnName);
        void     arrayInit(std::vector<uint8_t> &data, Type& fnType);
        void     parse2();
        bool     checkStackUnique(const char* str);

    public:
        inline Parser(::Parser& _p, Tree& _world) : p(_p), world(_world) { breakLabel=0; continueLabel=0; lastSwitch=0; curFn=0; }

        void     parse(unsigned step);
    };
}
