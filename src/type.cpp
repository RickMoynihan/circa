// Copyright 2008 Andrew Fischer

#include "common_headers.h"

#include "branch.h"
#include "builtins.h"
#include "errors.h"
#include "function.h"
#include "operations.h"
#include "term.h"
#include "type.h"

namespace circa {

Type::Type()
  : name(""),
    parentType(NULL),
    dataSize(sizeof(void*)),
    alloc(NULL),
    dealloc(NULL),
    duplicate(NULL),
    equals(NULL),
    compare(NULL),
    remapPointers(NULL),
    toString(NULL)
{
}

void
Type::addMemberFunction(std::string const &name, Term *function)
{
    // make sure argument 0 of the function matches this type
    if (as_type(as_function(function)->inputTypes[0]) != this)
        throw errors::InternalError("argument 0 of function doesn't match this type");

    this->memberFunctions.bind(function, name);
}

/*
const char * PARENT_FIELD_NAME = ":parent";

bool has_parent(Term *term)
{
    if (!is_compound_value(term))
        return false;
    return as_compound_type(term->type).getName(0) == PARENT_FIELD_NAME;
}

Term* get_parent(Term *term)
{
    assert(has_parent(term));
    return as_compound_value(term).getField(0);
}

Term* get_superclass(Term *type)
{
    assert(as_compound_type(type).getName(0) == SUPERCLASS_FIELD_NAME);
    return as_compound_type(type).getType(0);
}

Term* get_as_type(Term *term, Term *type)
{
    if (term->type == type)
        return term;

    if (has_superclass(type))
        return get_as_type(term, get_parent(type), get_superclass(type));

    return NULL;
}
*/

bool is_instance(Term *term, Term *type)
{
    // Special case during bootstrapping.
    if (CURRENTLY_BOOTSTRAPPING && type == NULL)
        return true;

    Term* actualType = term->type;

    while (actualType != NULL) {

        if (actualType == type)
            return true;

        actualType = as_type(actualType)->parentType;
    }

    return false;
}

void assert_instance(Term *term, Term *type)
{
    if (!is_instance(term, type))
        throw errors::TypeError(term, type);
}

bool is_type(Term *term)
{
    return is_instance(term, TYPE_TYPE);
}

Type* as_type(Term *term)
{
    assert_instance(term, TYPE_TYPE);
    return (Type*) term->value;
}

void Type_alloc(Term *caller)
{
    caller->value = new Type();
}

std::string Type_toString(Term *caller)
{
    return std::string("<Type " + as_type(caller)->name + ">");
}

void set_member_function(Term *type, std::string name, Term *function)
{
    Type* typeData = as_type(type);
    as_function(function);

    typeData->memberFunctions.bind(function, name);
}

Term* get_member_function(Term* type, std::string name)
{
    return as_type(type)->memberFunctions[name];
}

void unsafe_change_type(Term *term, Term *type)
{
    if (term->value == NULL) {
        change_type(term, type);
        return;
    }

    term->type = type;
}

void change_type(Term *term, Term *typeTerm)
{
    if (term->type == typeTerm)
        return;

    if (term->value != NULL)
        throw errors::InternalError("value is not NULL in change_type (possible memory leak)");

    Term* oldType = term->type;

    term->type = typeTerm;

    Type *type = as_type(typeTerm);

    if (type->alloc == NULL) {
        throw errors::InternalError(string("type ") + type->name + " has no alloc function");
    }

    type->alloc(term);
}

void specialize_type(Term *term, Term *type)
{
    if (term->type == type) {
        return;
    }

    if (term->type != ANY_TYPE)
        throw errors::TypeError(term, ANY_TYPE);

    change_type(term, type);
}

namespace type_private {

void empty_function(Term*) {}

}

Term* create_empty_type(Branch* branch)
{
    Term* term = create_constant(branch, TYPE_TYPE);
    Type* type = as_type(term);
    type->alloc = type_private::empty_function;
    type->dealloc = type_private::empty_function;
    return term;
}

} // namespace circa
