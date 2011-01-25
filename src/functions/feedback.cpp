// Copyright (c) Paul Hodge. See LICENSE file for license terms.

#include <circa.h>

namespace circa {
namespace feedback_function {

    CA_FUNCTION(evaluate)
    {
        // No-op
    }

    CA_FUNCTION(evaluate_apply_feedback)
    {
        //Branch& input = INPUT_TERM(0)->nestedContents;
        //FIXME refresh_training_branch(input, as_branch(OUTPUT));
    }

    void setup(Branch& kernel)
    {
        FEEDBACK_FUNC = import_function(kernel, evaluate, "feedback(any,any)");
        function_t::set_input_meta(FEEDBACK_FUNC, 0, true);

        #if 0
        import_function(kernel, evaluate_apply_feedback, "apply_feedback(Branch branch)->Branch "
            "'Experimental, creates terms to apply feedback in the given branch'");
        #endif
    }
}
}
