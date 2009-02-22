// Copyright 2008 Andrew Fischer

#ifndef CIRCA_CODE_ITERATOR_INCLUDED
#define CIRCA_CODE_ITERATOR_INCLUDED

#include "common_headers.h"

namespace circa {

class CodeIterator
{
    Branch* _topBranch;
    int _topIndex;
    CodeIterator* _subBranch;

public:
    CodeIterator(Branch* branch);
    ~CodeIterator();
    void reset(Branch* branch);
    Term* current();
    void advance();
    void postAdvance();

    Term* operator*() { return current(); }
    Term* operator->() { return current(); }

    bool finished()
    {
        return _topBranch == NULL;
    }

    int depth()
    {
        if (_subBranch == NULL)
            return 0;
        else
            return _subBranch->depth() + 1;
    }
};
    
} // namespace circa

#endif
