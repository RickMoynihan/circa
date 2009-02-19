// Copyright 2008 Paul Hodge

#include "common_headers.h"

#include "builtins.h"
#include "dictionary.h"
#include "testing.h"
#include "term.h"
#include "term_namespace.h"

namespace circa {
namespace container_tests {

void test_namespace()
{
    TermNamespace nspace;
    Term *term = new Term();

    nspace.bind(term, "a");
    test_assert(nspace.contains("a"));
    test_assert(nspace["a"] == term);

    Term *term2 = new Term();
    ReferenceMap remap;
    remap[term] = term2;
    nspace.remapPointers(remap);
    test_assert(nspace["a"] == term2);

    test_assert(nspace.contains("a"));
    remap[term2] = NULL;
    nspace.remapPointers(remap);
    test_assert(!nspace.contains("a"));
}

void test_list()
{
    RefList list;
    Term* term = new Term();
    Term* term2 = new Term();

    test_assert(list.count() == 0);

    list.append(term);
    list.append(term2);
    test_assert(list.count() == 2);
    test_assert(list[0] == term);
    test_assert(list[1] == term2);

    Term* term3 = new Term();
    ReferenceMap remap;
    remap[term] = term3;
    list.remapPointers(remap);
    test_assert(list.count() == 2);
    test_assert(list[0] == term3);
    test_assert(list[1] == term2);

    list.clear();

    test_assert(list.count() == 0);
}

void test_dictionary()
{
    Dictionary dict;

    test_assert(!dict.contains("a"));

    dict.addSlot("a", INT_TYPE);
    as_int(dict["a"]) = 4;
    dict.addSlot("b", INT_TYPE);
    as_int(dict["b"]) = 6;

    test_assert(dict.contains("a"));
    test_assert(dict.contains("b"));

    test_assert(as_int(dict["a"]) == 4);
    test_assert(as_int(dict["b"]) == 6);

    dict.clear();

    test_assert(!dict.contains("a"));
    test_assert(!dict.contains("b"));

    // duplication
    dict.addSlot("a", INT_TYPE);
    as_int(dict["a"]) = 55;
    Dictionary dup;
    dup.import(dict);
    test_assert(dup.contains("a"));
    test_assert(dup["a"] != dict["a"]);
    test_assert(as_int(dup["a"]) == 55);
}

void register_tests()
{
    REGISTER_TEST_CASE(container_tests::test_namespace);
    REGISTER_TEST_CASE(container_tests::test_list);
    REGISTER_TEST_CASE(container_tests::test_dictionary);
}

} // namespace container_tests

} // namespace circa
