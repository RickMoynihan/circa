// Copyright 2008 Andrew Fischer

#include "circa.h"
#include "function.h"

namespace circa {
namespace subroutine_apply_function {

    void evaluate(Term* caller)
    {
        recycle_value(caller->input(0), caller);
        std::string input = as_string(caller->input(1));

        Function& sub = as_function(caller);

        apply_statement(sub.subroutineBranch, input);
    }

    void setup(Branch& kernel)
    {
        import_c_function(kernel, evaluate,
            "subroutine-apply(Function, string) -> Function");
    }
}
}
