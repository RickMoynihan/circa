// Copyright (c) Paul Hodge. See LICENSE file for license terms.

#pragma once

#include "common_headers.h"

namespace circa {

// Puts 'errorTerm' into an error state, with the given message.
void error_occurred(EvalContext* context, Term* errorTerm, std::string const& message);

// Signal that an unexpected error has occurred. Depending on debug settings, this
// will either throw an exception or trigger an assert().
void internal_error(const char* message);
void internal_error(std::string const& message);

bool has_static_error(Term* term);
bool has_static_errors(Branch& branch);

std::string get_static_errors_formatted(Branch& branch);
std::string get_static_error_message(Term* term);

void print_static_error_formatted(Term* term, std::ostream& output);
void print_static_errors_formatted(Branch& branch, std::ostream& output);

void print_runtime_error_formatted(EvalContext& context, std::ostream& output);

enum StaticError {
    SERROR_NO_ERROR = 0,
    SERROR_NULL_FUNCTION,
    SERROR_WRONG_NUMBER_OF_INPUTS,
    SERROR_NULL_INPUT_TERM,
    SERROR_INPUT_TYPE_ERROR,
    SERROR_UNKNOWN_FUNCTION,
    SERROR_UNKNOWN_TYPE,
    SERROR_UNKNOWN_IDENTIFIER,
    SERROR_UNKNOWN_FIELD,
    SERROR_UNRECGONIZED_EXPRESSION
};

StaticError get_static_error(Term* term);
int count_static_errors(Branch& branch);

} // namespace circa
