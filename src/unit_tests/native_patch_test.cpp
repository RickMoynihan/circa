// Copyright (c) Andrew Fischer. See LICENSE file for license terms.

#include "unit_test_common.h"

#include "evaluation.h"
#include "fakefs.h"
#include "kernel.h"
#include "modules.h"
#include "native_patch.h"
#include "world.h"

namespace native_patch_test {

void my_add(caStack* stack)
{
    circa_set_int(circa_output(stack, 0),
            circa_int_input(stack, 0) + circa_int_input(stack, 1));
}

void my_5(caStack* stack)
{
    circa_set_int(circa_output(stack, 0), 5);
}

void my_6(caStack* stack)
{
    circa_set_int(circa_output(stack, 0), 6);
}

void patch_manually()
{
    World* world = global_world();

    // Run with an unpatched 'my_add'
    Block block;
    block.compile("def my_add(int a, int b) -> int { a + a }");
    block.compile("namespace ns { def my_add(int a, int b) -> int { a + a } }");
    block.compile("test_spy(my_add(1 2))");

    Stack stack;
    push_frame(&stack, &block);
    test_spy_clear();
    run_interpreter(&stack);

    test_equals(test_spy_get_results(), "[2]");

    // Create a patch for my_add
    NativePatch* patch = add_native_patch(world, "patch_manually");
    module_patch_function(patch, "my_add", my_add);
    native_patch_apply_patch(patch, &block);

    reset_stack(&stack);
    push_frame(&stack, &block);
    test_spy_clear();
    run_interpreter(&stack);

    test_equals(test_spy_get_results(), "[3]");

    remove_native_patch(world, "patch_manually");
}

void patch_manually_ns()
{
    World* world = global_world();

    // Similar to 'patch_manually' test, but with a namespaced name.
    Block block;
    block.compile("def f1() -> int { 1 }");
    block.compile("namespace ns_a { def f1() -> int { 1 } }");
    block.compile("namespace ns_b { namespace ns_a { def f1() -> int { 1 } } }");
    block.compile("test_spy(f1())");
    block.compile("test_spy(ns_a:f1())");
    block.compile("test_spy(ns_b:ns_a:f1())");

    NativePatch* patch = add_native_patch(world, "patch_manually_ns");
    module_patch_function(patch, "ns_a:f1", my_5);
    module_patch_function(patch, "ns_b:ns_a:f1", my_6);
    native_patch_apply_patch(patch, &block);

    Stack stack;
    push_frame(&stack, &block);
    test_spy_clear();
    run_interpreter(&stack);

    test_equals(test_spy_get_results(), "[1, 5, 6]");

    remove_native_patch(world, "patch_manually_ns");
}

void trigger_change()
{
    World* world = global_world();

    // Don't patch manually, add a change action and trigger it.
    Block* block = fetch_module(world, "trigger_change_test");
    block->compile("def f() -> int { 1 }");
    block->compile("test_spy(f())");

    NativePatch* patch = add_native_patch(world, "trigger_change_test");

    Stack stack;
    push_frame(&stack, block);
    test_spy_clear();
    run_interpreter(&stack);

    // First pass, patch not in effect.
    test_equals(test_spy_get_results(), "[1]");

    // Now, patch in effect.
    module_patch_function(patch, "f", my_5);
    native_patch_finish_change(patch);

    reset_stack(&stack);
    push_frame(&stack, block);
    test_spy_clear();
    run_interpreter(&stack);

    test_equals(test_spy_get_results(), "[5]");

    remove_native_patch(world, "trigger_change_test");
}

void new_function_patched_by_world()
{
    World* world = global_world();

    // First create the patch, as part of the global world.
    NativePatch* patch = add_native_patch(global_world(), "nativemod_block");
    module_patch_function(patch, "my_add", my_add);
    native_patch_finish_change(patch);

    // Now create our function, it should get patched instantly.
    Block* block = fetch_module(global_world(), "nativemod_block");
    block->compile("def my_add(int a, int b) -> int { a + a }");
    block->compile("test_spy(my_add(1 2))");

    Stack stack;
    push_frame(&stack, block);
    test_spy_clear();
    run_interpreter(&stack);

    test_equals(test_spy_get_results(), "[3]");

    remove_native_patch(world, "trigger_change_test");
}

void patch_manually_public_api()
{
    FakeFilesystem fs;
    fs.set("Module.ca", "def my_5() -> int; my_5() -> output");

    caWorld* world = global_world();
    caBlock* module = circa_load_module_from_file(world, "Module", "Module.ca");

    caNativePatch* npatch = circa_create_native_patch(world, "Module");
    circa_patch_function(npatch, "my_5", my_5);
    circa_finish_native_patch(npatch);

    caStack* stack = circa_alloc_stack(world);
    circa_push_module(stack, "Module");
    circa_run(stack);

    test_assert(stack);
    test_equals(circa_output(stack, 0), "5");

    circa_dealloc_stack(stack);
}

void register_tests()
{
    REGISTER_TEST_CASE(native_patch_test::patch_manually);
    REGISTER_TEST_CASE(native_patch_test::patch_manually_ns);
    REGISTER_TEST_CASE(native_patch_test::trigger_change);
    REGISTER_TEST_CASE(native_patch_test::new_function_patched_by_world);
    REGISTER_TEST_CASE(native_patch_test::patch_manually_public_api);
}

}
