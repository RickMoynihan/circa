// Copyright 2008 Andrew Fischer

#include "circa.h"

namespace circa {
namespace if_function {

    void evaluate(Term* caller)
    {
        bool cond = as_bool(caller->input(0));

        if (cond) {
            Branch& branch = as_branch(caller);
            evaluate_branch(branch);
        }
    }

    std::string toSourceString(Term* term)
    {
        std::stringstream result;
        result << "if ";
        result << get_source_of_input(term, 0);
        result << "\n";

        Branch& branch = *get_inner_branch(term);
        result << get_branch_source(branch);

        result << "end";
        return result.str();
    }

    void setup(Branch& kernel)
    {
        IF_FUNC = import_function(kernel, evaluate, "if(bool) : Branch");
        as_function(IF_FUNC).toSourceString = toSourceString;
    }
}
}
