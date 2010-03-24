// Copyright (c) 2007-2010 Paul Hodge. All rights reserved.

#include "circa.h"

namespace circa {
namespace comment_function {

    void evaluate(EvalContext*, Term* caller)
    {
    }

    std::string toSourceString(Term* term)
    {
        return get_comment_string(term);
    }

    void formatSource(RichSource* source, Term* term)
    {
        append_phrase(source, get_comment_string(term).c_str(), term, richsource::COMMENT);
    }

    void setup(Branch& kernel)
    {
        COMMENT_FUNC = import_function(kernel, evaluate, "comment()");
        function_t::get_attrs(COMMENT_FUNC).toSource = toSourceString;
        function_t::get_attrs(COMMENT_FUNC).formatSource = formatSource;
    }
}
}
