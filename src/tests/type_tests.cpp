// Copyright 2008 Paul Hodge

#include "common_headers.h"

#include "circa.h"

namespace circa {
namespace type_tests {

void compound_types()
{
    Branch branch;

    Term* MyType = branch.eval("type MyType { int myint, string astr }");
    test_assert(MyType != NULL);
    test_assert(is_type(MyType));
    test_assert(as_type(MyType).prototype.length() == 2);
    test_assert(as_type(MyType).prototype[0]->name == "myint");
    test_assert(as_type(MyType).prototype[0]->type == INT_TYPE);
    test_assert(as_type(MyType).findFieldIndex("myint") == 0);
    test_assert(as_type(MyType).prototype[1]->name == "astr");
    test_assert(as_type(MyType).prototype[1]->type == STRING_TYPE);
    test_assert(as_type(MyType).findFieldIndex("astr") == 1);

    // instanciation
    Term* inst = branch.eval("inst = MyType()");
    test_assert(inst != NULL);
    test_assert(inst->type = MyType);
    test_assert(inst->value != NULL);
    test_assert(as_branch(inst)[0]->asInt() == 0);
    test_assert(as_branch(inst)[1]->asString() == "");

    // field access on a brand new type
    Term* astr = branch.eval("inst.astr");
    test_assert(is_string(astr));
    test_equals(as_string(astr), "");

    // field assignment
    Term *inst2 = branch.eval("inst.astr = 'hello'");
    test_assert(inst2);
    test_assert(as_branch(inst2)[1]->asString() == "hello");
    test_assert(inst2->type == MyType); // type specialization

    // field access of recently assigned value
    Term* astr2 = branch.eval("inst.astr");
    test_assert(is_string(astr2));
    test_equals(as_string(astr2), "hello");
}

void type_declaration()
{
    Branch branch;
    Term* myType = branch.eval("type MyType { string a, int b } ");

    test_assert(as_type(myType).numFields() == 2);
    test_assert(as_type(myType).prototype[0]->name == "a");
    test_assert(as_type(myType).prototype[0]->type == STRING_TYPE);
    test_assert(as_type(myType).prototype[1]->name == "b");
    test_assert(as_type(myType).prototype[1]->type == INT_TYPE);

    test_assert(as_type(myType).alloc != NULL);
    test_assert(as_type(myType).assign != NULL);

    Term* instance = branch.eval("mt = MyType()");

    test_assert(is_value_alloced(instance));
}

void test_value_fits_type()
{
    Branch branch;

    Term* a = int_value(branch, 5);
    test_assert(value_fits_type(a, INT_TYPE));
    test_assert(value_fits_type(a, FLOAT_TYPE));
    test_assert(!value_fits_type(a, STRING_TYPE));
    test_assert(value_fits_type(a, ANY_TYPE));

    Term* t1 = branch.eval("type t1 { int a, float b }");
    Term* t2 = branch.eval("type t2 { int a }");
    Term* t3 = branch.eval("type t3 { int a, float b, string c }");
    Term* t4 = branch.eval("type t4 { float a, int b }");

    Term* v1 = branch.eval("[1, 2.0]");
    test_assert(value_fits_type(v1, t1));
    test_assert(!value_fits_type(v1, t2));
    test_assert(!value_fits_type(v1, t3));
    test_assert(!value_fits_type(v1, t4));

    Term* v2 = branch.eval("['hello' 2.0]");
    test_assert(!value_fits_type(v2, t1));
    test_assert(!value_fits_type(v2, t2));
    test_assert(!value_fits_type(v2, t3));
    test_assert(!value_fits_type(v2, t4));

    Term* v3 = branch.eval("[1]");
    test_assert(!value_fits_type(v3, t1));
    test_assert(value_fits_type(v3, t2));
    test_assert(!value_fits_type(v3, t3));
    test_assert(!value_fits_type(v3, t4));
    
    Term* v4 = branch.eval("[]");
    test_assert(!value_fits_type(v4, t1));
    test_assert(!value_fits_type(v4, t2));
    test_assert(!value_fits_type(v4, t3));
    test_assert(!value_fits_type(v4, t4));

    Term* v5 = branch.eval("[1 2]");
    test_assert(value_fits_type(v5, t1)); // coercion into a compound value
    test_assert(!value_fits_type(v5, t2));
    test_assert(!value_fits_type(v5, t3));
    test_assert(value_fits_type(v5, t4)); // coercion again
}

void test_is_native_type()
{
    test_assert(is_native_type(INT_TYPE));
    test_assert(is_native_type(STRING_TYPE));
    test_assert(is_native_type(BOOL_TYPE));
    test_assert(is_native_type(FLOAT_TYPE));
    test_assert(is_native_type(TYPE_TYPE));
}

void test_to_string()
{
    // Test on some native types
    test_equals(to_string(INT_TYPE), "<NativeType int>");
    test_equals(to_string(FLOAT_TYPE), "<NativeType float>");
    test_equals(to_string(BOOL_TYPE), "<NativeType bool>");
    test_equals(to_string(STRING_TYPE), "<NativeType string>");
    test_equals(to_string(TYPE_TYPE), "<NativeType Type>");

    // to_string for compound types is handled in source_repro_tests.cpp
}

void test_default_values()
{
    Branch branch;

    Term* i = int_value(branch, 5);
    assign_value_to_default(i);
    test_assert(i->asInt() == 0);

    Term* f = float_value(branch, 5);
    assign_value_to_default(f);
    test_assert(f->asFloat() == 0);

    Term* s = string_value(branch, "hello");
    assign_value_to_default(s);
    test_assert(s->asString() == "");
}

void type_inference_for_get_index()
{
    Branch branch;
    branch.eval("l = [1 2 3]");
    Term* x = branch.compile("x = l[0]");
    test_assert(branch);
    test_assert(x->type == INT_TYPE);

    branch.clear();
    branch.eval("l = [1 2.0 3]");
    x = branch.compile("x = l[0]");
    test_assert(branch);
    test_assert(x->type == FLOAT_TYPE);

    branch.clear();
    branch.eval("l = [1 2.0 'three']");
    x = branch.compile("x = l[0]");
    test_assert(x->type == ANY_TYPE);
}

void register_tests()
{
    REGISTER_TEST_CASE(type_tests::compound_types);
    REGISTER_TEST_CASE(type_tests::type_declaration);
    REGISTER_TEST_CASE(type_tests::test_value_fits_type);
    REGISTER_TEST_CASE(type_tests::test_is_native_type);
    REGISTER_TEST_CASE(type_tests::test_to_string);
    REGISTER_TEST_CASE(type_tests::test_default_values);
    REGISTER_TEST_CASE(type_tests::type_inference_for_get_index);
}

} // namespace type_tests
} // namespace circa
