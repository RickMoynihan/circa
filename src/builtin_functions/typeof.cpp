// Copyright 2008 Paul Hodge

#include "circa.h"

namespace circa {
namespace typeof_function {

    void evaluate(Term* caller)
    {
        caller->value = caller->input(0)->type->value;
        as_type(caller).refCount++;
    }

    void setup(Branch& kernel)
    {
        Term* func = import_function(kernel, evaluate, "typeof(any) -> Type");
        as_function(func).pureFunction = true;
    }
}
}