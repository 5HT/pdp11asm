// PDP11 Assembler (c) 08-01-2017 ALeksey Morozov (aleksey.f.morozov@gmail.com)

#pragma once

#include "c_asm_8080.h"
#include "c_tree.h"
#include "compiler.h"
#include "queue"

namespace C
{

class Compiler8080 {
public:
    class IfOpt {
    public:
        bool ifTrue;
        bool ok;
        unsigned label;
    };

    Asm8080 out;
    Compiler& compiler;
    Tree& tree;

    std::queue<size_t> writePtrs;

    Function* curFn;
    unsigned  inStack;
    unsigned  returnLabel;
    unsigned  usedRegs;

    Compiler8080(Compiler& _compiler, Tree& _world);    
    void compileFunction(Function* f);
    void compileJump(NodeVar* v, bool ifTrue, unsigned label);
    unsigned pushAcc1();
    void popAcc(unsigned usedRegs);
    void popAccSwap(char s, unsigned usedRegs);
    void compileVar(Node* n, unsigned d, IfOpt* ifOpt=0);
    void compileBlock(Node* n);

    void compileOperatorSet(NodeOperator* r, unsigned d);
    void compileOperatorAlu(NodeOperator* r, unsigned d);
    void compileMonoOperatorIncDec(NodeMonoOperator* o, unsigned d);

public:
    void start(unsigned step);
};

}
