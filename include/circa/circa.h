// Copyright (c) Paul Hodge. See LICENSE file for license terms.

#ifndef CIRCA_H_INCLUDED
#define CIRCA_H_INCLUDED

#include <stdbool.h>

// Public API

#ifdef __cplusplus
extern "C" {
#endif

// -- Circa Types --

// a World holds miscellaneous shared runtime information. A process should create one
// World that is used across the program.
//
// (NOTE: Currently, no more than one World can be created per process due to the use of
// statics, this will be fixed in the future).
typedef struct caWorld caWorld;

// a Stack holds the interpreter's current state, including a list of frames (activation
// records). Each Stack corresponds to one lightweight thread (not OS thread).
typedef struct caStack caStack;

// a Value is a variant value. It holds two pointers, one pointer to the Type object and
// one to the value's data. For some types (such as integers, floats, booleans), the data
// section holds the actual value and not a pointer.
typedef struct caValue caValue;

// a Branch is a section of compiled code. It contains a list of Terms and some other
// metadata. Each Term may itself contain a nested Branch.
typedef struct caBranch caBranch;

// a Term is one unit of compiled code. Each term has a function and a list of inputs, and
// some other metadata. A term may also have a nested Branch.
typedef struct caTerm caTerm;

// a Type holds data for a single Circa type, including a name, handlers for
// initialization and destruction, and various other handlers and metadata.
typedef struct caType caType;

// a Function holds data for a single Circa function, including a name, the function's
// definition (stored as a Branch), and various other metadata. Each Function has an
// EvaluateFunc which is triggered when the function is called.
typedef struct caFunction caFunction;

// a Name is an alias for an integer, used to reference an interned string.
typedef int caName;

// EvaluateFunc is the signature for a C evaluation function. The function will access the
// stack to read inputs (if any), possibly perform some action, and write output values
// (if any) back to the stack.
typedef void (*caEvaluateFunc)(caStack* stack);

// ReleaseFunc is the signature for a C function that runs when a user object is about to
// be destroyed. This is used for 'handle' values.
typedef void (*caReleaseFunc)(caValue* value);

// -- Setting up the Circa environment --

// Create a caWorld. This should be done once at the start of the program.
caWorld* circa_initialize();

// Uninitialize a caWorld. This call is entirely optional, but using it will make memory-
// leak checking tools happier.
void circa_shutdown(caWorld*);

// Set up a POSIX file handler as a file source. For more control over file sources, see
// circa/file.h
void circa_use_standard_filesystem(caWorld* world);

// Add a module search path. This is used when processing 'import' statements.
void circa_add_module_search_path(caWorld* world, const char* path);

// Execute the command-line handler with the given arguments.
int circa_run_command_line(caWorld* world, int argc, const char* args[]);

// Load a module by opening the given filename as a source file.
caBranch* circa_load_module_from_file(caWorld* world,
                                      const char* module_name,
                                      const char* filename);

// -- Controlling the Interpreter --

// Allocate a new Stack object.
caStack* circa_alloc_stack(caWorld* world);

// Deallocate a Stack object.
void circa_dealloc_stack(caStack* stack);

// Execute the named module.
void circa_run_module(caStack* stack, const char* moduleName);

// Execute the given function with the given inputs. 'inputs' must be a list, these
// values are used as the function's arguments. The function's outputs are then written
// back to your 'inputs' list.
void circa_run_function(caStack* stack, caFunction* function, caValue* inputs);

// Signal that an error has occurred.
void circa_raise_error(caStack* stack, const char* msg);

// Return whether a runtime error occurred.
bool circa_has_error(caStack* stack);

// Clear a runtime error from the stack.
void circa_clear_error(caStack* stack);

// Print a human-readable description of the stack's error to stdout.
void circa_print_error_to_stdout(caStack* stack);

// -- Fetching Inputs & Outputs --

// Retrieve the given input value. This value may not be modified.
caValue* circa_input(caStack* stack, int index);

// Read the given input index as an integer.
// Equivalent to: circa_int(circa_input(stack, index))
int circa_int_input(caStack* stack, int index);

// Read the given input index as a string
// Equivalent to: circa_string(circa_input(stack, index))
const char* circa_string_input(caStack* stack, int index);

// Read the given input index as a float
// Equivalent to: circa_float(circa_input(stack, index))
float circa_float_input(caStack* stack, int index);

// Fetch the given output value. This value should be modified.
caValue* circa_output(caStack* stack, int index);

// Initialize the given output value, using the default create() function on the
// Term's declared type. Also, returns the output value for convenient writing.
// This function is unnecessary if you write to the output using one of the
// circa_set_XXX functions.
caValue* circa_create_default_output(caStack* stack, int index);

// Fetch the Term that is currently being evaluated.
caTerm* circa_current_term(caStack* stack);

// Fetch the Branch for the function that is currently being evaluated
caBranch* circa_callee_branch(caStack* stack);

// -- Tagged Values --

// Allocate a new caValue container on the heap. This will call circa_init_value for you.
caValue* circa_alloc_value();

// Deallocate a caValue that was created with circa_alloc_value.
void circa_dealloc_value(caValue* value);

// Initialize a newly allocated caValue container. This must be called if you allocate
// the caValue yourself. (such as putting it on the stack).
void circa_init_value(caValue* value);

// Copy a value. Some types may implement this as a lightweight copy, so you will need
// to follow the rules for circa_touch when using the copy.
void circa_copy(caValue* source, caValue* dest);

// "Touch" a value, indicating that you are about to start modifying its contents. This
// is only necessary when modifying the elements of a container type (such as a List).
void circa_touch(caValue* value);

// -- Accessors --

// Check the type of a caValue.
bool circa_is_bool(caValue* value);
bool circa_is_branch(caValue* value);
bool circa_is_error(caValue* value);
bool circa_is_function(caValue* value);
bool circa_is_float(caValue* value);
bool circa_is_int(caValue* value);
bool circa_is_list(caValue* value);
bool circa_is_name(caValue* value);
bool circa_is_null(caValue* value);
bool circa_is_number(caValue* value);
bool circa_is_string(caValue* value);
bool circa_is_type(caValue* value);

// Read the value from a caValue.
bool        circa_bool(caValue* value);
caBranch*   circa_branch(caValue* value);
float       circa_float(caValue* value);
caFunction* circa_function(caValue* value);
int         circa_int(caValue* value);
const char* circa_string(caValue* value);
caType*     circa_type(caValue* value);
void        circa_vec2(caValue* vec2, float* xOut, float* yOut);
void        circa_vec3(caValue* vec3, float* xOut, float* yOut, float* zOut);
void        circa_vec4(caValue* vec4, float* xOut, float* yOut, float* zOut, float* wOut);

// Read the pointer value from a caValue. This call will do dereferencing: if the caValue
// is actually a Handle then we'll dereference the handle.
void*       circa_get_pointer(caValue* value);

// Fetch a caValue as a float (converting it from an int if necessary)
float circa_to_float(caValue* value);

// Access an element by index. There are certain rules for using this result:
//   - The element must not be modified, unless you know that you have a writable copy
//     of the container. (Use circa_touch to obtain a writeable copy)
//   - The element pointer becomes invalid when the owning container is resized.
//
// When in doubt, make a copy (circa_copy) of the returned value and use that.
//
caValue* circa_index(caValue* value, int index);

// Number of elements in a list value.
int circa_count(caValue* container);

// -- Writing to a caValue --


// Assign to a caValue.
void circa_set_bool(caValue* container, bool value);
void circa_set_float(caValue* container, float value);
void circa_set_int(caValue* container, int value);
void circa_set_null(caValue* container);
void circa_set_pointer(caValue* container, void* ptr);
void circa_set_term(caValue* container, caTerm* term);
void circa_set_string(caValue* container, const char* str);
void circa_set_typed_pointer(caValue* container, caType* type, void* ptr);
void circa_set_vec2(caValue* container, float x, float y);
void circa_set_vec3(caValue* container, float x, float y, float z);
void circa_set_vec4(caValue* container, float x, float y, float z, float w);

// Assign to a string, with the given length. 'str' does not need to be NULL-terminated
// here.
void circa_set_string_size(caValue* container, const char* str, int size);

// Append to a string
void circa_string_append(caValue* container, const char* str);

// Initialize a list. The container will have length 'numElements' and each element will
// be NULL.
void circa_set_list(caValue* container, int numElements);

// Append a value to a list and return the newly appended value. This return value may be
// modified.
caValue* circa_append(caValue* list);

// Resize a list.
void circa_resize(caValue* list, int count);

// Assign a point.
void circa_set_point(caValue* point, float x, float y);

// -- Handle Values --

// Retrive a value from a Handle
caValue* circa_handle_get_value(caValue* handle);

// Assign a value to a Handle
void circa_handle_set(caValue* container, caValue* value, caReleaseFunc releaseFunc);

void circa_handle_set_object(caValue* handle, void* object, caReleaseFunc releaseFunc);

void* circa_handle_get_object(caValue* handle);

// Assign a Value using the Type's default create() handler.
void circa_create_value(caValue* value, caType* type);

// -- Names --

// Fetch the interned name for the given string. Returns 0 if the name doesn't exist.
caName circa_name(const char* str);

// Fetch the string for an interned name
const char* circa_name_to_string(caName name);

// -- String Representation --

// Load a Circa value from a string representation. The result will be written to 'out'.
// If there is a parsing error, an error value will be saved to 'out'. (the caller should
// check for this).
void circa_parse_string(const char* str, caValue* out);

// Write a string representation of 'value' to 'out'.
void circa_to_string_repr(caValue* value, caValue* out);

// -- Code Reflection --

// Find a Term by name, looking in the given branch.
caTerm* circa_find_term(caBranch* branch, const char* name);

// Find a Function by name, looking in the given branch.
caFunction* circa_find_function(caBranch* branch, const char* name);

// Find a Type by name, looking in the given branch.
caType* circa_find_type(caBranch* branch, const char* name);

// Retrive the nth input Term for the stack's current Term. May return NULL.
// This is equivalent to: circa_term_get_input(circa_current_term(stack), index)
caTerm* circa_input_term(caStack* stack, int index);

// Get a Term from a Branch by index.
caTerm* circa_get_term(caBranch* branch, int index);

// Get the Branch contents for a given Term. This may return NULL.
caBranch* circa_nested_branch(caTerm* term);

// Get the Branch contents for a term with the given name.
caBranch* circa_get_nested_branch(caBranch* branch, const char* name);

// Get the Branch contents for a function
caBranch* circa_function_contents(caFunction* func);

// Access the fixed value of a value() Term. Returns NULL if Term is not a value.
caValue* circa_term_value(caTerm* term);

int circa_term_get_index(caTerm* term);

// Fetch the number of inputs for the given term.
int circa_term_num_inputs(caTerm* term);

// Fetch the nth input for the given term. May return NULL.
caTerm* circa_term_get_input(caTerm* term, int index);

// Fetch the Term's declared type.
caType* circa_term_declared_type(caTerm* term);

// -- Code Building --

// Install an evaluation function to the given named function. Returns the container Term.
// Returns NULL if the name was not found.
caTerm* circa_install_function(caBranch* branch, const char* name, caEvaluateFunc func);

typedef struct caFunctionBinding
{
    const char* name;
    caEvaluateFunc func;
} caFunctionBinding;

// Install a series of C function bindings. This will treat 'bindingList' as an array
// that is terminanted with a NULL caFunctionBinding.
void circa_install_function_list(caBranch* branch, const caFunctionBinding* bindingList);

// Install an evaluation function to the given Function.
void circa_func_set_evaluate(caFunction* func, caEvaluateFunc evaluate);

// Create a new Function value with the given name. Returns the created Function.
caFunction* circa_declare_function(caBranch* branch, const char* name);

// Create a new value() term with the given name. Returns the term's value, which is safe
// to modify.
caValue* circa_declare_value(caBranch* branch, const char* name);

// -- Debugging Helpers --

// 'dump' commands will print a representation to stdout
void circa_dump_s(caStack* stack);
void circa_dump_b(caBranch* branch);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
