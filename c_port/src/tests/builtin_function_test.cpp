
#include "common_headers.h"

#include "circa.h"
#include <tests/common.h>

namespace circa {
namespace builtin_function_test {

void test_math()
{
    Branch* branch = new Branch();

    Term* two = constant_int(branch, 2);
    Term* three = constant_int(branch, 3);
    Term* negative_one = constant_int(branch, -1);
    Term* add_f = get_global("add");
    Term* mult_f = get_global("mult");

    test_assert(as_int(exec_function(branch, add_f, TermList(two,three))) == 5);
    test_assert(as_int(exec_function(branch, add_f, TermList(two,negative_one))) == 1);

    test_assert(as_int(exec_function(branch, mult_f, TermList(two,three))) == 6);
    test_assert(as_int(exec_function(branch, mult_f, TermList(negative_one,three))) == -3);

    test_assert(as_int(quick_exec_function(branch, "mult(5,3)")) == 15);
}

void test_string()
{
    Branch* branch = new Branch();

    test_assert(as_string(quick_exec_function(branch, "concat(\"hello \", \"world\")"))
            == "hello world");
}

void test_bool()
{
    Branch* branch = new Branch();

    test_assert(as_string(quick_exec_function(branch, "if-expr(true, 'a, 'b)")) == "a");
    test_assert(as_string(quick_exec_function(branch, "if-expr(false, 'a, 'b)")) == "b");
}

void all_tests()
{
    test_math();
    test_string();
    test_bool();
}

} // namespace builtin_function_test
} // namespace circa
