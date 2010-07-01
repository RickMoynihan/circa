// Copyright (c) 2007-2010 Paul Hodge. All rights reserved.

#include "builtins.h"
#include "circa.h"

namespace circa {
namespace unknown_function_function {

    CA_FUNCTION(evaluate)
    {
    }

    void setup(Branch& kernel)
    {
        Term* main_func = import_function(kernel, evaluate, "unknown_function(any...) -> any");

        UNKNOWN_FUNCTION = main_func;
    }
}
}
