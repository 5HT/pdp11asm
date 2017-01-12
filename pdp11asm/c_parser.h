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

        struct Var
        {
            int         stackOff;
            std::string name;
            Type        type;
            inline Var() { stackOff=-1; }
        };

        NodeLabel* breakLabel, *continueLabel;
        NodeSwitch* lastSwitch;
        std::vector<Var> stackVars;
        Function* curFn;

        bool p_ifToken(const std::map<std::string,int>& hash, int& n);
        bool p_ifToken(const std::vector<std::string>& strs, int& o);
        bool p_ifToken(const std::vector<Var>& a, int& o);
        bool p_ifToken(const std::list<Function>& a, int& o);
        Operator findOperator(int level, int& l);
        uint32_t readConst(Type& to);
        uint16_t readConstU16();

        void parseStruct(Struct& s, int m);
        NodeIf*  allocNodeIf(NodeVar* _cond, Node* _t, Node* _f);
        NodeJmp* allocJmp(NodeLabel* _label, NodeVar* _cond, bool _ifZero);
        NodeJmp* allocJmp(NodeLabel* _label);
        NodeVar* nodeMonoOperator(NodeVar* a, MonoOperator o);
        NodeVar* nodeAddr(NodeVar* x);
        NodeVar* getStackVar(Var& x);
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
        NodeVar* nodeOperator2(Type type, Operator o, NodeVar* a, NodeVar* b, NodeVar* cond);
        Node*    readCommand1();
        void     getRemark(std::string& out);
        Function*parseFunction(Type& retType, const std::string& fnName);
        void     arrayInit(std::vector<char>& data, Type& fnType);
        void     parse2();

    public:
        inline Parser(::Parser& _p, Tree& _world) : p(_p), world(_world) { breakLabel=0; continueLabel=0; lastSwitch=0; curFn=0; }

        void     parse(unsigned step);
    };
}
