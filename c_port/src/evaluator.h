#ifndef CIRCA__EVALUATOR__INCLUDED
#define CIRCA__EVALUATOR__INCLUDED

#include "common_headers.h"

namespace circa {

class Evaluator {

    struct Scope {
        Term* callingTerm;
        Branch* branch;
        int next;

        Scope() : callingTerm(NULL), branch(NULL), next(0) {}
        virtual void onClose() {}
    };

    struct SubroutineScope : public Scope {
        virtual void onClose();
    };

    std::stack<Scope> mStack;

public:
    void evaluate(Term* term);
    void runNextInstruction();

};

}

#endif
