
#include "common_headers.h"

#include "parser/token.h"
#include "parser/token_stream.h"
#include "bootstrapping.h"
#include "branch.h"
#include "builtin_functions.h"
#include "errors.h"
#include "function.h"
#include "operations.h"
#include "structs.h"
#include "subroutine.h"
#include "term.h"
#include "type.h"

namespace circa {

Branch* KERNEL = NULL;
Term* CONST_GENERATOR = NULL;
Term* BUILTIN_INT_TYPE = NULL;
Term* BUILTIN_FLOAT_TYPE = NULL;
Term* BUILTIN_BOOL_TYPE = NULL;
Term* BUILTIN_STRING_TYPE = NULL;
Term* BUILTIN_TYPE_TYPE = NULL;
Term* BUILTIN_FUNCTION_TYPE = NULL;
Term* BUILTIN_CODEUNIT_TYPE = NULL;
Term* BUILTIN_SUBROUTINE_TYPE = NULL;
Term* BUILTIN_STRUCT_DEFINITION_TYPE = NULL;
Term* BUILTIN_BRANCH_TYPE = NULL;
Term* BUILTIN_ANY_TYPE = NULL;
Term* BUILTIN_VOID_TYPE = NULL;
Term* BUILTIN_REFERENCE_TYPE = NULL;
Term* BUILTIN_LIST_TYPE = NULL;
Term* CONSTANT_0 = NULL;
Term* CONSTANT_1 = NULL;
Term* CONSTANT_2 = NULL;

void empty_execute_function(Term*) { }
void empty_alloc_function(Term*) { }
void empty_function(Term*) { }

void const_generator(Term* caller)
{
    Function *output = as_function(caller);
    Type* type = as_type(caller->inputs[0]);
    output->name = "const-" + type->name;
    output->outputType = caller->inputs[0];
    output->execute = empty_execute_function;
}

Term* get_global(string name)
{
    if (KERNEL->containsName(name))
        return KERNEL->getNamed(name);

    throw errors::KeyError(name);
}


void bootstrap_kernel()
{
    KERNEL = new Branch();

    // Create const-generator function
    Term* CONST_GENERATOR = new Term();
    Function_alloc(CONST_GENERATOR);
    as_function(CONST_GENERATOR)->name = "const-generator";
    as_function(CONST_GENERATOR)->pureFunction = true;
    as_function(CONST_GENERATOR)->execute = const_generator;
    KERNEL->bindName(CONST_GENERATOR, "const-generator");

    // Create const-Type function
    Term* constTypeFunc = new Term();
    constTypeFunc->function = CONST_GENERATOR;
    Function_alloc(constTypeFunc);
    as_function(constTypeFunc)->name = "const-Type";
    as_function(constTypeFunc)->pureFunction = true;

    // Create Type type
    Term* typeType = new Term();
    BUILTIN_TYPE_TYPE = typeType;
    typeType->function = constTypeFunc;
    typeType->type = typeType;
    Type_alloc(typeType);
    as_type(typeType)->name = "Type";
    as_type(typeType)->alloc = Type_alloc;
    KERNEL->bindName(typeType, "Type");

    // Implant the Type type
    set_input(constTypeFunc, 0, typeType);
    as_function(CONST_GENERATOR)->inputTypes.setAt(0, typeType);
    as_function(constTypeFunc)->outputType = typeType;

    // Create const-Function function
    Term* constFuncFunc = new Term();
    constFuncFunc->function = CONST_GENERATOR;
    Function_alloc(constFuncFunc);
    as_function(constFuncFunc)->name = "const-Function";
    as_function(constFuncFunc)->pureFunction = true;
    KERNEL->bindName(constFuncFunc, "const-Function");

    // Implant const-Function
    CONST_GENERATOR->function = constFuncFunc;

    // Create Function type
    Term* functionType = new Term();
    BUILTIN_FUNCTION_TYPE = functionType;
    functionType->function = constTypeFunc;
    functionType->type = typeType;
    Type_alloc(functionType);
    as_type(functionType)->name = "Function";
    as_type(functionType)->alloc = Function_alloc;
    as_type(functionType)->dealloc = Function_dealloc;
    KERNEL->bindName(functionType, "Function");

    // Implant Function type
    set_input(CONST_GENERATOR, 0, typeType);
    set_input(constFuncFunc, 0, functionType);
    CONST_GENERATOR->type = functionType;
    constFuncFunc->type = functionType;
    constTypeFunc->type = functionType;
    as_function(CONST_GENERATOR)->outputType = functionType;
    as_function(constFuncFunc)->outputType = functionType;

    // Don't let these terms get updated
    CONST_GENERATOR->needsUpdate = false;
    constFuncFunc->needsUpdate = false;
    constTypeFunc->needsUpdate = false;
    functionType->needsUpdate = false;
    typeType->needsUpdate = false;
}

void int_alloc(Term* caller)
{
    caller->value = new int;
}
void int_dealloc(Term* caller)
{
    delete (int*) caller->value;
}
void int_copy(Term* source, Term* dest)
{
    as_int(dest) = as_int(source);
}

void float_alloc(Term* caller)
{
    caller->value = new float;
}
void float_dealloc(Term* caller)
{
    delete (float*) caller->value;
}
void float_copy(Term* source, Term* dest)
{
    as_float(dest) = as_float(source);
}

void string_alloc(Term* caller)
{
    caller->value = new string;
}

void string_dealloc(Term* caller)
{
    delete (string*) caller->value;
}

void bool_alloc(Term* caller)
{
    caller->value = new bool;
}

void bool_dealloc(Term* caller)
{
    delete (bool*) caller->value;
}

void bool_copy(Term* source, Term* dest)
{
    as_bool(dest) = as_bool(source);
}

void int_tostring(Term* caller)
{
    std::stringstream strm;
    strm << as_int(caller->inputs[0]);
    as_string(caller) = strm.str();
}
void float_tostring(Term* caller)
{
    std::stringstream strm;
    strm << as_float(caller->inputs[0]);
    as_string(caller) = strm.str();
}
void string_tostring(Term* caller)
{
    as_string(caller) = as_string(caller->inputs[0]);
}
void string_copy(Term* source, Term* dest)
{
    as_string(dest) = as_string(source);
}

void bool_tostring(Term* caller)
{
    if (as_bool(caller))
        as_string(caller) = "true";
    else
        as_string(caller) = "false";
}

void create_builtin_types()
{
    BUILTIN_STRING_TYPE = quick_create_type(KERNEL, "string",
            string_alloc,
            string_dealloc,
            string_copy,
            string_tostring);
    BUILTIN_INT_TYPE = quick_create_type(KERNEL, "int",
            int_alloc, int_dealloc, int_copy, int_tostring);
    BUILTIN_FLOAT_TYPE = quick_create_type(KERNEL, "float",
            float_alloc, float_dealloc, float_copy, float_tostring);
    BUILTIN_BOOL_TYPE = quick_create_type(KERNEL, "bool",
            bool_alloc, bool_dealloc, bool_copy, bool_tostring);
    BUILTIN_ANY_TYPE = quick_create_type(KERNEL, "any",
            empty_function, empty_function, NULL);
    BUILTIN_VOID_TYPE = quick_create_type(KERNEL, "void",
            empty_function, empty_function, NULL);
    BUILTIN_REFERENCE_TYPE = quick_create_type(KERNEL, "Reference",
            empty_function, empty_function, NULL);
}

void initialize_constants()
{
    CONSTANT_0 = constant_int(KERNEL, 0);
    CONSTANT_1 = constant_int(KERNEL, 1);
    CONSTANT_2 = constant_int(KERNEL, 2);
}

void initialize()
{
    try {
        bootstrap_kernel();
        create_builtin_types();

        initialize_constants();

        // Do initialize_term first
        initialize_term(KERNEL);

        initialize_branch(KERNEL);
        initialize_builtin_functions(KERNEL);
        initialize_functions(KERNEL);
        initialize_structs(KERNEL);
        initialize_subroutine(KERNEL);

        initialize_bootstrapped_code(KERNEL);

    } catch (errors::CircaError& e)
    {
        std::cout << "An error occured while initializing." << std::endl;
        std::cout << e.message() << std::endl;
        exit(1);
    }
}

} // namespace circa
