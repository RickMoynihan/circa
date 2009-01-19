// Copyright 2008 Paul Hodge

#include "common_headers.h"

#include "branch.h"
#include "function.h"
#include "introspection.h"
#include "runtime.h"
#include "term.h"
#include "type.h"

namespace circa {

bool is_value(Term* term)
{
    assert_good_pointer(term);
    return term->function->function == VALUE_FUNCTION_GENERATOR;
}

std::string get_short_local_name(Term* term)
{
    if (term == NULL)
        return "NULL";
    if (term->name != "")
        return term->name;

    std::stringstream stream;
    stream << "#" << term->globalID;
    return stream.str();
}

void print_raw_term(Term* term, std::ostream &output)
{
    if (term == NULL) {
        output << "<NULL>";
        return;
    }

    std::string name = term->name;
    std::string funcName = get_short_local_name(term->function);
    std::string typeName = term->type->name;

    output << "#" << term->globalID;

    if (name != "")
        output << " " << name << "";

    output << ": ";

    output << funcName << "(";

    bool first_input = true;
    for (unsigned int input_index=0; input_index < term->inputs.count(); input_index++) {
        Term* input = term->inputs[input_index];
        if (!first_input) output << ", ";
        output << get_short_local_name(input);
        first_input = false;
    }

    output << ")";

    if (term->type != VOID_TYPE)
        output << " -> " << typeName;

    if (term->hasError())
        output << " *" << term->getErrorMessage() << "*";

    output << std::endl;
}

void print_raw_branch(Branch& branch, std::ostream &output)
{
    for (int i=0; i < branch.numTerms(); i++) {
        Term* term = branch[i];

        print_raw_term(term, output);
    }
}

void print_terms(ReferenceList const& list, std::ostream &output)
{
    for (unsigned int i=0; i < list.count(); i++) {
        print_raw_term(list[i], output);
    }
}

ReferenceList list_all_pointers(Term* term)
{
    ReferenceList result;

    struct AppendPointersToList : PointerVisitor
    {
        ReferenceList& list;
        AppendPointersToList(ReferenceList &_list) : list(_list) {}

        virtual void visitPointer(Term* term) {
            if (term != NULL)
                list.appendUnique(term);
        }
    };

    AppendPointersToList visitor(result);
    visit_pointers(term, visitor);

    return result;
}

bool function_allows_term_reuse(Function &function)
{
    if ((function.stateType != VOID_TYPE) && (function.stateType != NULL))
        return false;

    if (!function.pureFunction)
        return false;

    return true;
}

bool is_equivalent(Term* target, Term* function, ReferenceList const& inputs)
{
    if (target->function != function)
        return false;

    if (!function_allows_term_reuse(as_function(function)))
        return false;

    // Check inputs
    unsigned int numInputs = target->inputs.count();

    if (numInputs != inputs.count())
        return false;

    for (unsigned int i=0; i < numInputs; i++) {
        if (target->inputs[i] != inputs[i]) {
            return false;
        }
    }

    return true;
}

Term* find_equivalent(Term* function, ReferenceList const& inputs)
{
    if (!function_allows_term_reuse(as_function(function))) {
        return NULL;
    }

    // Check users of each input
    for (unsigned int input_i=0; input_i < inputs.count(); input_i++) {
        Term* input = inputs[input_i];
        if (input == NULL)
            continue;

        for (unsigned int user_i=0; user_i < input->users.count(); user_i++) {
            Term* user = input->users[user_i];

            if (is_equivalent(user, function, inputs))
                return user;
        }
    }

    // Check users of function
    for (unsigned int i=0; i < function->users.count(); i++) {
        Term* user = function->users[i];
        if (is_equivalent(user, function, inputs))
            return user;
    }

    return NULL;
}

void print_runtime_errors(Branch& branch, std::ostream& output)
{
    for (int i=0; i < branch.numTerms(); i++) {
        Term *term = branch[i];

        if (term == NULL)
            continue;

        if (term->hasError()) {
            output << "error on " << term->name << ": " << term->getErrorMessage() << std::endl;
        }
    }
}

void check_for_compile_error(Term* term, std::string& errorMessage)
{
    if (term->function == UNKNOWN_FUNCTION) {
        errorMessage = std::string("Unknown function: " + as_string(term->state));
    }
}

bool has_compile_errors(Branch& branch)
{
    for (int i=0; i < branch.numTerms(); i++) {
        Term* term = branch[i];
        std::string message;
        check_for_compile_error(term, message);
        if (message != "")
            return true;
    }
    return false;
}

std::vector<std::string> get_compile_errors(Branch& branch)
{
    std::vector<std::string> results;

    for (int i=0; i < branch.numTerms(); i++) {
        Term* term = branch[i];
        std::string message;

        check_for_compile_error(term, message);

        if (message != "")
            results.push_back(message);
    }

    return results;
}

void print_compile_errors(Branch& branch, std::ostream& output)
{
    for (int i=0; i < branch.numTerms(); i++) {
        Term* term = branch[i];
        std::string message;

        check_for_compile_error(term, message);

        if (message != "")
            output << message << std::endl;
    }
}


} // namespace circa
