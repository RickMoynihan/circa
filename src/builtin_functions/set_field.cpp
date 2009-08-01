// Copyright 2008 Paul Hodge

#include "circa.h"

namespace circa {
namespace set_field_function {

    void evaluate(Term* caller)
    {
        assign_value(caller->input(0), caller);

        std::string name = caller->input(1)->asString();
        int index = as_type(caller->input(0)->type).findFieldIndex(name);
        assign_value(caller->input(2), as_branch(caller)[index]);
    }

    Term* specializeType(Term* caller)
    {
        return caller->input(0)->type;
    }

    std::string toSourceString(Term* term)
    {
        std::stringstream out;
        out << get_source_of_input(term, 0);
        out << ".";
        out << term->input(1)->asString();
        out << " =";
        out << get_source_of_input(term, 2);
        return out.str();
    }

    void setup(Branch& kernel)
    {
        SET_FIELD_FUNC = import_function(kernel, evaluate,
                "set_field(any, string, any) : any");
        function_t::get_specialize_type(SET_FIELD_FUNC) = specializeType;
        function_t::get_to_source_string(SET_FIELD_FUNC) = toSourceString;
    }
}
}
