// Copyright (c) Paul Hodge. See LICENSE file for license terms.

#include "common_headers.h"

#include "circa_internal.h"
#include "dict.h"

namespace circa {
namespace declared_state_function {

    CA_FUNCTION(get_declared_state)
    {
        TValue* value = INPUT(0);
        TValue* output = OUTPUT;

        // Try to cast 'value' to the declared type.
        if (value != NULL && !is_null(value)) {
            bool cast_success = cast(value, declared_type(CALLER), output);

            // If this cast succeeded then we're done. If it failed then continue on
            // to use a default value.
            if (cast_success)
                return;
        }

        // We couldn't use the input value, push a frame to evaluate the default value.
        Branch* contents = nested_contents(CALLER);

        if (contents->length() > 0) {
            set_symbol(OUTPUT, InProgress);
            push_frame(CONTEXT, nested_contents(CALLER));
            return;
        }

        // Otherwise, reset to the type's default value
        if (declared_type(CALLER) == &ANY_T)
            set_null(OUTPUT);
        else
            create(declared_type(CALLER), OUTPUT);

#if 0
        // Try to use default_value from an input.
        TValue* defaultValue = INPUT(1);
        if (defaultValue != NULL) {
            bool cast_success = cast(defaultValue, declared_type(CALLER), output);

            if (!cast_success) {
                std::stringstream msg;
                msg << "Couldn't cast default value to type " <<
                    declared_type(CALLER)->name;
                RAISE_ERROR(msg.str().c_str());
            }
        } else {

            // Otherwise, reset to the type's default value
            create(declared_type(CALLER), output);
        }
#endif
    }

    void formatSource(StyledSource* source, Term* term)
    {
        append_phrase(source, "state ", term, TK_STATE);

        if (term->hasProperty("syntax:explicitType")) {
            append_phrase(source, term->stringProp("syntax:explicitType"),
                    term, phrase_type::TYPE_NAME);
            append_phrase(source, " ", term, TK_WHITESPACE);
        }

        append_phrase(source, term->name.c_str(), term, phrase_type::TERM_NAME);

        Term* defaultValue = NULL;

        if (nested_contents(term)->length() > 0) {
            defaultValue = nested_contents(term)->getFromEnd(0)->input(0);
            if (defaultValue->boolPropOptional("hidden", false))
                defaultValue = defaultValue->input(0);
        }

        if (defaultValue != NULL) {
            append_phrase(source, " = ", term, phrase_type::UNDEFINED);
            if (defaultValue->name != "")
                append_phrase(source, get_relative_name_at(term, defaultValue),
                        term, phrase_type::TERM_NAME);
            else
                format_term_source(source, defaultValue);
        }
    }

    void setup(Branch* kernel)
    {
        DECLARED_STATE_FUNC = import_function(kernel, get_declared_state,
            "declared_state(state any value) -> any");
        as_function(DECLARED_STATE_FUNC)->formatSource = formatSource;
    }
}
}
