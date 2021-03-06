// Copyright (c) Andrew Fischer. See LICENSE file for license terms.

#include "common_headers.h"

#include "block.h"
#include "building.h"
#include "code_iterators.h"
#include "kernel.h"
#include "function.h"
#include "inspection.h"
#include "list.h"
#include "static_checking.h"
#include "reflection.h"
#include "names.h"
#include "term.h"
#include "type.h"

namespace circa {

void append_static_error(List* errors, Term* term, const char* type)
{
    caValue* item = set_list(list_append(errors), 3);
    set_term_ref(list_get(item, 0), term);
    set_string(list_get(item, 1), type);
    set_int(list_get(item, 2), -1);
}

void append_static_error_for_input(List* errors, Term* term, const char* type,
        int inputIndex)
{
    caValue* item = set_list(list_append(errors), 3);
    set_term_ref(list_get(item, 0), term);
    set_string(list_get(item, 1), type);
    set_int(list_get(item, 2), inputIndex);
}

void check_input_for_static_error(List* errors, Term* term, int index)
{
    int effectiveIndex = index;

    bool varArgs = term_has_variable_args(term);
    if (varArgs)
        effectiveIndex = 0;

    Term* input = term->input(index);
    Term* placeholder = term_get_input_placeholder(term, effectiveIndex);
    bool meta = placeholder->boolProp("meta", false);
    bool optional = placeholder->boolProp("optional", false);

    if (input == NULL) {
        if (!meta && !optional)
            append_static_error_for_input(errors, term, "null_input", index);
        return;
    }

    // Check type
    // Currently disabled
#if 0
    Type* type = placeholder->type;
    if (term_output_never_satisfies_type(input, type))
        append_static_error_for_input(errors, term, "type_mismatch", index);
#endif
}

void check_term_for_static_error(List* errors, Term* term)
{
    if (term->function == NULL)
        return append_static_error(errors, term, "null_function");

    if (term->function == FUNCS.unknown_function)
        return append_static_error(errors, term, "unknown_function");

    if (!is_function(term->function))
        return append_static_error(errors, term, "not_a_function");

    bool varArgs = term_has_variable_args(term);
    int expectedInputCount = term_count_input_placeholders(term);

    // Check # of inputs
    if (!varArgs && (term->numInputs() != expectedInputCount))
        return append_static_error(errors, term, "wrong_input_count");

    for (int input=0; input < term->numInputs(); input++)
        check_input_for_static_error(errors, term, input);

    if (!is_function(term->function) && !is_an_unknown_identifier(term->function))
        return append_static_error(errors, term, "not_a_function");

    // Unknown identifier
    if (term->function == FUNCS.unknown_identifier)
        return append_static_error(errors, term, "unknown_identifier");

    // Unrecognized expression
    if (term->function == FUNCS.unrecognized_expression)
        return append_static_error(errors, term, "unrecognized_expression");

    if (term->function == FUNCS.static_error)
        return append_static_error(errors, term, to_string(term_value(term->input(0))).c_str());
}

void check_for_static_errors(List* errors, Block* block)
{
    for (BlockIterator it(block); it.unfinished(); ++it)
        check_term_for_static_error(errors, *it);
}

void update_static_error_list(Block* block)
{
    List errors;
    check_for_static_errors(&errors, block);
    if (errors.empty())
        set_null(&block->staticErrors);
    else
        swap(&errors, &block->staticErrors);
}

bool has_static_error(Term* term)
{
    List errors;
    check_term_for_static_error(&errors, term);
    return !errors.empty();
}

bool has_static_errors(Block* block)
{
    update_static_error_list(block);
    return has_static_errors_cached(block);
}

bool has_static_errors_cached(Block* block)
{
    return !is_null(&block->staticErrors);
}

int count_static_errors(Block* block)
{
    update_static_error_list(block);
    if (is_null(&block->staticErrors))
        return 0;
    return list_length(&block->staticErrors);
}

void format_static_error(caValue* error, caValue* stringOutput)
{
    Term* term = as_term_ref(list_get(error, 0));
    const char* type = as_cstring(list_get(error, 1));
    int inputIndex = as_int(list_get(error, 2));

    std::stringstream out;
    out << get_short_location(term) << " ";
    
    // Convert 'type' to a readable string
    if (strcmp(type, "not_a_function") == 0)
        out << "Not a function: " << term->function->name;
    else if (strcmp(type, "unknown_type") == 0)
        out << "Unknown type: " << term->name;
    else if (strcmp(type, "unknown_identifier") == 0)
        out << "Unknown identifier: " << term->name;
    else if (strcmp(type, "unrecognized_expression") == 0)
        out << "Unrecognized expression: " << term->stringProp("message", "");
    else if (strcmp(type, "wrong_input_count") == 0) {
        int funcNumInputs = term_count_input_placeholders(term);
        int actualCount = term->numInputs();
        if (actualCount > funcNumInputs)
            out << "Too many inputs (" << actualCount << "), function "
                << term->function->name << " expects only " << funcNumInputs;
        else
            out << "Too few inputs (" << actualCount << "), function "
                << term->function->name << " expects " << funcNumInputs;
    } else if (strcmp(type, "null_function") == 0)
        out << "NULL function reference";
    else if (strcmp(type, "unknown_function") == 0)
        out << "Unknown function: " << term->stringProp("syntax:functionName", "");
    else if (strcmp(type, "null_input") == 0)
        out << "NULL input reference for input " << inputIndex;
    else if (strcmp(type, "type_mismatch") == 0) {
        out << "Type mismatch for input " << inputIndex << ": ";
        Term* input = term->input(inputIndex);
        if (input->name != "")
            out << "'" << input->name << "'";
        else
            out << "The input expression";
        out << " has type " << as_cstring(&input->type->name) << ", but function "
            << term->function->name << " expects type "
            << as_cstring(&function_get_input_type(term->function, inputIndex)->name);
    }
    else if (term->function == FUNCS.static_error)
        out << to_string(term_value(term->input(0)));
    else
        //out << "(unrecognized error type: " << type << ")";
        out << type;

    set_string(stringOutput, out.str());
}

void print_static_error(caValue* value, std::ostream& out)
{
    Value str;
    format_static_error(value, &str);
    out << as_cstring(&str);
}

bool print_static_errors_formatted(caValue* result, std::ostream& out)
{
    int count = list_length(result);

    if (count == 0)
        return false;

    out << count << " static error";
    if (count != 1) out << "s";
    out << ":\n";

    for (int i=0; i < count; i++) {
        print_static_error(list_get(result, i), out);
        out << std::endl;
    }
    return true;
}

bool print_static_errors_formatted(Block* block, caValue* out)
{
    std::stringstream strm;
    if (!print_static_errors_formatted(block, strm))
        return false;
    set_string(out, strm.str());
    return true;
}

bool print_static_errors_formatted(Block* block, std::ostream& out)
{
    update_static_error_list(block);
    if (!is_list(&block->staticErrors))
        return false;
    return print_static_errors_formatted(&block->staticErrors, out);
}

bool print_static_errors_formatted(Block* block)
{
    return print_static_errors_formatted(block, std::cout);
}

void print_static_error(Term* term, std::ostream& out)
{
    // delete this
    List result;
    check_term_for_static_error(&result, term);
    if (!result.empty())
        print_static_error(result[0], out);
}

std::string get_static_errors_formatted(Block* block)
{
    std::stringstream out;
    print_static_errors_formatted(block, out);
    return out.str();
}

std::string get_static_error_message(Term* term)
{
    std::stringstream out;
    print_static_error(term, out);
    return out.str();
}

void mark_static_error(Term* term, const char* msg)
{
    term->setStringProp("error", msg);
}

void mark_static_error(Term* term, caValue* error)
{
    term->setProp("error", error);
}

} // namespace circa
