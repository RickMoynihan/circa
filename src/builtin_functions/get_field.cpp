// Copyright 2008 Paul Hodge

#include "circa.h"

namespace circa {
namespace get_field_function {

    void evaluate(Term* caller)
    {
        int index = as_int(caller->state);
        Term* field = as_branch(caller->input(0))[index];

        assign_value(field, caller);
    }

    void setup(Branch& kernel)
    {
        GET_FIELD_FUNC = import_function(kernel, evaluate, "get_field(any) -> any");
        as_function(GET_FIELD_FUNC).stateType = INT_TYPE;
        as_function(GET_FIELD_FUNC).pureFunction = true;
    }
}
}