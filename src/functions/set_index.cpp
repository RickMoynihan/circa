// Copyright (c) Paul Hodge. See LICENSE file for license terms.

#include "circa.h"

namespace circa {
namespace set_index_function {

    CA_FUNCTION(evaluate)
    {
        copy(INPUT(0), OUTPUT);
        touch(OUTPUT);
        int index = INT_INPUT(1);
        copy(INPUT(2), OUTPUT->getIndex(index));
    }

    Term* specializeType(Term* caller)
    {
        //FIXME
        return LIST_TYPE;
        //return caller->input(0)->type;
    }

    void setup(Branch& kernel)
    {
        SET_INDEX_FUNC = import_function(kernel, evaluate,
                "set_index(any, int, any) -> List");
        function_t::get_specialize_type(SET_INDEX_FUNC) = specializeType;
    }
}
}
