// Copyright 2008 Andrew Fischer

#ifndef CIRCA_BUILTINS_INCLUDED
#define CIRCA_BUILTINS_INCLUDED

#include "common_headers.h"

namespace circa {

extern Branch* KERNEL;

extern Term* VALUE_FUNCTION_GENERATOR;
extern Term* INT_TYPE;
extern Term* FLOAT_TYPE;
extern Term* BOOL_TYPE;
extern Term* STRING_TYPE;
extern Term* TYPE_TYPE;
extern Term* FUNCTION_TYPE;
extern Term* CODEUNIT_TYPE;
extern Term* BRANCH_TYPE;
extern Term* ANY_TYPE;
extern Term* VOID_TYPE;
extern Term* VOID_PTR_TYPE;
extern Term* REF_TYPE;
extern Term* LIST_TYPE;
extern Term* MAP_TYPE;
extern Term* VAR_INT;
extern Term* VAR_FLOAT;
extern Term* VAR_STRING;
extern Term* VAR_BOOL;
extern Term* CONSTANT_TRUE;
extern Term* CONSTANT_FALSE;
extern Term* UNKNOWN_FUNCTION;
extern Term* UNKNOWN_IDENTIFIER_FUNC;
extern Term* UNKNOWN_TYPE_FUNC;
extern Term* UNRECOGNIZED_EXPRESSION_FUNC;
extern Term* APPLY_FEEDBACK;
extern Term* ADD_FUNC;
extern Term* SUB_FUNC;
extern Term* MULT_FUNC;
extern Term* DIV_FUNC;
extern Term* ALIAS_FUNC;
extern Term* COMMENT_FUNC;
extern Term* INT_TO_FLOAT_FUNC;
extern Term* COPY_FUNC;
extern Term* IF_FUNC;
extern Term* STATEFUL_VALUE_FUNC;
extern Term* ASSIGN_FUNC;
extern Term* SET_FIELD_FUNC;

Term* get_global(std::string name);
void empty_evaluate_function(Term* caller);
void initialize();
void shutdown();


} // namespace circa

#endif
