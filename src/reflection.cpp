// Copyright (c) Andrew Fischer. See LICENSE file for license terms.

#include "common_headers.h"
#include "circa/circa.h"

#include "block.h"
#include "building.h"
#include "code_iterators.h"
#include "evaluation.h"
#include "hashtable.h"
#include "importing.h"
#include "inspection.h"
#include "generic.h"
#include "kernel.h"
#include "modules.h"
#include "reflection.h"
#include "source_repro.h"
#include "stateful_code.h"
#include "static_checking.h"
#include "term.h"
#include "tagged_value.h"
#include "type.h"

#include "value_iterator.h"

namespace circa {
    
void set_term_ref(caValue* val, Term* term)
{
    change_type(val, TYPES.term);
    val->value_data.ptr = term;
}

Term* as_term_ref(caValue* val)
{
    ca_assert(is_term_ref(val));
    return (Term*) val->value_data.ptr;
}

bool is_term_ref(caValue* val)
{
    return val->value_type == TYPES.term;
}

void block_ref(caStack* stack)
{
    Term* input0 = (Term*) circa_caller_input_term(stack, 0);
    Block* block = input0->nestedContents;
    if (block != NULL) {
        gc_mark_object_referenced(&block->header);
    }
    set_block(circa_output(stack, 0), block);
}

void term_ref(caStack* stack)
{
    caTerm* term = circa_caller_input_term(stack, 0);
    set_term_ref(circa_output(stack, 0), (Term*) term);
}

void update_all_code_references_in_value(caValue* value, Block* oldBlock, Block* newBlock)
{
    for (ValueIterator it(value); it.unfinished(); it.advance()) {
        caValue* val = *it;
        if (is_ref(val)) {
            set_term_ref(val, translate_term_across_blockes(as_term_ref(val),
                oldBlock, newBlock));
            
        } else if (is_block(val)) {

            // If this is just a reference to 'oldBlock' then simply update it to 'newBlock'.
            if (as_block(val) == oldBlock) {
                set_block(val, newBlock);
                continue;
            }

            // Noop on null block.
            if (as_block(val) == NULL)
                continue;

            // Noop if block has no owner.
            Term* oldTerm = as_block(val)->owningTerm;
            if (oldTerm == NULL)
                continue;

            Term* newTerm = translate_term_across_blockes(oldTerm, oldBlock, newBlock);
            if (newTerm == NULL) {
                set_block(val, NULL);
                continue;
            }

            set_block(val, newTerm->nestedContents);
        }
    }
}

void Block__dump(caStack* stack)
{
    dump(as_block(circa_input(stack, 0)));
}

void Block__input(caStack* stack)
{
    Block* block = as_block(circa_input(stack, 0));
    set_term_ref(circa_output(stack, 0),
        get_input_placeholder(block, circa_int_input(stack, 1)));
}
void Block__inputs(caStack* stack)
{
    Block* block = as_block(circa_input(stack, 0));
    caValue* output = circa_output(stack, 0);
    set_list(output, 0);
    for (int i=0;; i++) {
        Term* term = get_input_placeholder(block, i);
        if (term == NULL)
            break;
        set_term_ref(list_append(output), term);
    }
}
void Block__is_null(caStack* stack)
{
    Block* block = as_block(circa_input(stack, 0));
    set_bool(circa_output(stack, 0), block == NULL);
}
void Block__output(caStack* stack)
{
    Block* block = as_block(circa_input(stack, 0));
    set_term_ref(circa_output(stack, 0),
        get_output_placeholder(block, circa_int_input(stack, 1)));
}
void Block__outputs(caStack* stack)
{
    Block* block = as_block(circa_input(stack, 0));
    caValue* output = circa_output(stack, 0);
    set_list(output, 0);
    for (int i=0;; i++) {
        Term* term = get_output_placeholder(block, i);
        if (term == NULL)
            break;
        set_term_ref(list_append(output), term);
    }
}
void Block__owner(caStack* stack)
{
    Block* block = as_block(circa_input(stack, 0));
    if (block == NULL) {
        set_term_ref(circa_output(stack, 0), NULL);
        return;
    }

    set_term_ref(circa_output(stack, 0), block->owningTerm);
}
void Block__property(caStack* stack)
{
    Block* block = as_block(circa_input(stack, 0));

    caValue* value = block_get_property(block, circa_input(stack, 1));

    if (value == NULL)
        set_null(circa_output(stack, 0));
    else
        copy(value, circa_output(stack, 0));
}
void Block__properties(caStack* stack)
{
    Block* block = as_block(circa_input(stack, 0));

    if (is_null(&block->properties))
        set_hashtable(circa_output(stack, 0));
    else
        copy(&block->properties, circa_output(stack, 0));
}

void Block__format_source(caStack* stack)
{
    Block* block = as_block(circa_input(stack, 0));

    caValue* output = circa_output(stack, 0);
    circa_set_list(output, 0);
    format_block_source((caValue*) output, block);
}

void Block__format_function_heading(caStack* stack)
{
    Block* block = as_block(circa_input(stack, 0));
    caValue* output = circa_output(stack, 0);
    circa_set_list(output, 0);
    function_format_header_source(output, block);
}

#if 0
void Block__get_documentation(caStack* stack)
{
    Block* block = as_block(circa_input(stack, 0));

    caValue* out = circa_output(stack, 0);
    set_list(out, 0);

    for (int i=0; i < block->length(); i++) {
        Term* term = block->get(i);
        if (is_input_placeholder(term))
            continue;

        if (is_comment(term))
            set_string(list_append(out), term->stringProp("comment"));
    }
}
#endif

void Block__has_static_error(caStack* stack)
{
    Block* block = as_block(circa_input(stack, 0));
    set_bool(circa_output(stack, 0), has_static_errors_cached(block));
}

void Block__get_static_errors(caStack* stack)
{
    Block* block = as_block(circa_input(stack, 0));

    if (is_null(&block->staticErrors))
        set_list(circa_output(stack, 0), 0);
    else
        copy(&block->staticErrors, circa_output(stack, 0));
}

void Block__get_static_errors_formatted(caStack* stack)
{
    Block* block = as_block(circa_input(stack, 0));
    if (block == NULL)
        return circa_output_error(stack, "NULL block");

    if (is_null(&block->staticErrors))
        set_list(circa_output(stack, 0), 0);

    caValue* errors = &block->staticErrors;
    caValue* out = circa_output(stack, 0);
    set_list(out, circa_count(errors));
    for (int i=0; i < circa_count(out); i++)
        format_static_error(circa_index(errors, i), circa_index(out, i));
}

void Block__call(caStack* stack)
{
    Block* block = as_block(circa_input(stack, 0));
    if (block == NULL)
        return circa_output_error(stack, "NULL block");

    caValue* inputs = circa_input(stack, 1);
    ca_assert(is_list(inputs));
    push_frame_with_inputs(stack, block, inputs);
}

// Reflection

void Block__terms(caStack* stack)
{
    Block* block = as_block(circa_input(stack, 0));
    if (block == NULL)
        return circa_output_error(stack, "NULL block");

    caValue* out = circa_output(stack, 0);
    set_list(out, block->length());

    for (int i=0; i < block->length(); i++)
        set_term_ref(circa_index(out, i), block->get(i));
}

void Block__version(caStack* stack)
{
    Block* block = as_block(circa_input(stack, 0));
    if (block == NULL)
        return circa_output_error(stack, "NULL block");
    set_int(circa_output(stack, 0), block->version);
}

void Block__walk_terms(caStack* stack)
{
    Block* block = as_block(circa_input(stack, 0));
    if (block == NULL)
        return circa_output_error(stack, "NULL block");

    caValue* out = circa_output(stack, 0);
    set_list(out, 0);
    for (BlockIterator it(block); it.unfinished(); it.advance())
        set_term_ref(list_append(out), *it);
}

void Block__get_term(caStack* stack)
{
    Block* block = as_block(circa_input(stack, 0));
    if (block == NULL)
        return circa_output_error(stack, "NULL block");

    int index = circa_int_input(stack, 1);
    set_term_ref(circa_output(stack, 0), block->get(index));
}

bool is_considered_config(Term* term)
{
    if (term == NULL) return false;
    if (term->name == "") return false;
    if (!is_value(term)) return false;
    if (is_declared_state(term)) return false;
    if (is_hidden(term)) return false;
    if (is_function(term)) return false;
    if (is_type(term)) return false;

    return true;
}

void Block__list_configs(caStack* stack)
{
    Block* block = as_block(circa_input(stack, 0));
    if (block == NULL)
        return circa_output_error(stack, "NULL block");

    caValue* output = circa_output(stack, 0);

    for (int i=0; i < block->length(); i++) {
        Term* term = block->get(i);
        if (is_considered_config(term))
            set_term_ref(circa_append(output), term);
    }
}

void Block__functions(caStack* stack)
{
    Block* block = as_block(circa_input(stack, 0));
    if (block == NULL)
        return circa_output_error(stack, "NULL block");

    caValue* output = circa_output(stack, 0);
    set_list(output, 0);

    for (BlockIteratorFlat it(block); it.unfinished(); it.advance()) {
        Term* term = *it;
        if (is_function(term)) {
            set_block(list_append(output), function_contents(as_function(term)));
        }
    }
}

void Block__file_signature(caStack* stack)
{
    Block* block = as_block(circa_input(stack, 0));
    if (block == NULL)
        return circa_output_error(stack, "NULL block");

    caValue* fileOrigin = block_get_file_origin(block);
    if (fileOrigin == NULL)
        set_null(circa_output(stack, 0));
    else
    {
        caValue* output = set_list(circa_output(stack, 0), 2);
        copy(list_get(fileOrigin, 1), list_get(output, 0));
        copy(list_get(fileOrigin, 2), list_get(output, 1));
    }
}

void Block__find_term(caStack* stack)
{
    Block* block = as_block(circa_input(stack, 0));
    if (block == NULL)
        return circa_output_error(stack, "NULL block");

    Term* term = block->get(circa_string_input(stack, 1));

    set_term_ref(circa_output(stack, 0), term);
}

void Block__statements(caStack* stack)
{
    Block* block = (Block*) circa_block(circa_input(stack, 0));
    if (block == NULL)
        return circa_output_error(stack, "NULL block");

    caValue* out = circa_output(stack, 0);

    circa_set_list(out, 0);

    for (int i=0; i < block->length(); i++)
        if (is_statement(block->get(i)))
            circa_set_term(circa_append(out), (caTerm*) block->get(i));
}

void Block__link(caStack* stack)
{
    Block* self = (Block*) circa_block(circa_input(stack, 0));
    Block* source = (Block*) circa_block(circa_input(stack, 1));

    block_link_missing_functions(self, source);
}

void Term__name(caStack* stack)
{
    Term* t = as_term_ref(circa_input(stack, 0));
    if (t == NULL)
        return circa_output_error(stack, "NULL reference");
    set_string(circa_output(stack, 0), t->name);
}
void Term__to_string(caStack* stack)
{
    Term* t = as_term_ref(circa_input(stack, 0));
    if (t == NULL)
        return circa_output_error(stack, "NULL reference");
    set_string(circa_output(stack, 0), circa::to_string(term_value(t)));
}
void Term__to_source_string(caStack* stack)
{
    Term* t = as_term_ref(circa_input(stack, 0));
    if (t == NULL)
        return circa_output_error(stack, "NULL reference");
    set_string(circa_output(stack, 0), get_term_source_text(t));
}
void Term__format_source(caStack* stack)
{
    Term* t = as_term_ref(circa_input(stack, 0));
    if (t == NULL)
        return circa_output_error(stack, "NULL reference");
    caValue* output = circa_output(stack, 0);
    circa_set_list(output, 0);
    format_term_source(output, t);
}
void Term__format_source_normal(caStack* stack)
{
    Term* t = as_term_ref(circa_input(stack, 0));
    if (t == NULL)
        return circa_output_error(stack, "NULL reference");
    caValue* output = circa_output(stack, 0);
    circa_set_list(output, 0);
    format_term_source_normal(output, t);
}
void Term__function(caStack* stack)
{
    Term* t = as_term_ref(circa_input(stack, 0));
    if (t == NULL)
        return circa_output_error(stack, "NULL reference");
    set_block(circa_output(stack, 0), function_contents(as_function(t->function)));
}
void Term__type(caStack* stack)
{
    Term* t = as_term_ref(circa_input(stack, 0));
    if (t == NULL)
        return circa_output_error(stack, "NULL reference");
    set_type(circa_output(stack, 0), t->type);
}
void Term__assign(caStack* stack)
{
    Term* target = as_term_ref(circa_input(stack, 0));
    if (target == NULL) {
        circa_output_error(stack, "NULL reference");
        return;
    }

    caValue* source = circa_input(stack, 1);

    circa::copy(source, term_value(target));

    // Probably should update term->type at this point.
}

void Term__value(caStack* stack)
{
    Term* target = as_term_ref(circa_input(stack, 0));
    if (target == NULL) {
        circa_output_error(stack, "NULL reference");
        return;
    }

    copy(term_value(target), circa_output(stack, 0));
}

int tweak_round(double a) {
    return int(a + 0.5);
}

void Term__tweak(caStack* stack)
{
    Term* t = as_term_ref(circa_input(stack, 0));
    if (t == NULL)
        return circa_output_error(stack, "NULL reference");

    int steps = tweak_round(to_float(circa_input(stack, 1)));

    caValue* val = term_value(t);

    if (steps == 0)
        return;

    if (is_float(val)) {
        float step = get_step(t);

        // Do the math like this so that rounding errors are not accumulated
        float new_value = (tweak_round(as_float(val) / step) + steps) * step;
        set_float(val, new_value);

    } else if (is_int(val))
        set_int(val, as_int(val) + steps);
    else
        circa_output_error(stack, "Ref is not an int or number");
}

void Term__asint(caStack* stack)
{
    Term* t = as_term_ref(circa_input(stack, 0));
    if (t == NULL) {
        circa_output_error(stack, "NULL reference");
        return;
    }
    if (!is_int(term_value(t))) {
        circa_output_error(stack, "Not an int");
        return;
    }
    set_int(circa_output(stack, 0), as_int(term_value(t)));
}
void Term__asfloat(caStack* stack)
{
    Term* t = as_term_ref(circa_input(stack, 0));
    if (t == NULL) {
        circa_output_error(stack, "NULL reference");
        return;
    }
    
    set_float(circa_output(stack, 0), to_float(term_value(t)));
}
void Term__input(caStack* stack)
{
    Term* t = as_term_ref(circa_input(stack, 0));
    if (t == NULL) {
        circa_output_error(stack, "NULL reference");
        return;
    }
    int index = circa_int_input(stack, 1);
    if (index >= t->numInputs())
        set_term_ref(circa_output(stack, 0), NULL);
    else
        set_term_ref(circa_output(stack, 0), t->input(index));
}
void Term__inputs(caStack* stack)
{
    Term* t = as_term_ref(circa_input(stack, 0));
    if (t == NULL)
        return circa_output_error(stack, "NULL reference");

    caValue* output = circa_output(stack, 0);
    circa_set_list(output, t->numInputs());

    for (int i=0; i < t->numInputs(); i++)
        set_term_ref(circa_index(output, i), t->input(i));
}
void Term__num_inputs(caStack* stack)
{
    Term* t = as_term_ref(circa_input(stack, 0));
    if (t == NULL) {
        circa_output_error(stack, "NULL reference");
        return;
    }
    set_int(circa_output(stack, 0), t->numInputs());
}
void Term__parent(caStack* stack)
{
    Term* t = as_term_ref(circa_input(stack, 0));
    if (t == NULL) {
        circa_output_error(stack, "NULL reference");
        return;
    }
    set_block(circa_output(stack, 0), t->owningBlock);
}
void Term__contents(caStack* stack)
{
    Term* t = as_term_ref(circa_input(stack, 0));
    if (t == NULL) {
        circa_output_error(stack, "NULL reference");
        return;
    }
    set_block(circa_output(stack, 0), t->nestedContents);
}
void Term__is_null(caStack* stack)
{
    Term* t = as_term_ref(circa_input(stack, 0));
    set_bool(circa_output(stack, 0), t == NULL);
}

void Term__source_location(caStack* stack)
{
    Term* t = as_term_ref(circa_input(stack, 0));
    if (t == NULL)
        return circa_output_error(stack, "NULL reference");

    circa_set_vec4(circa_output(stack, 0),
        t->sourceLoc.col, t->sourceLoc.line,
        t->sourceLoc.colEnd, t->sourceLoc.lineEnd);
}
void Term__location_string(caStack* stack)
{
    Term* t = as_term_ref(circa_input(stack, 0));
    if (t == NULL)
        return circa_output_error(stack, "NULL reference");

    set_string(circa_output(stack, 0), get_short_location(t).c_str());
}
void Term__global_id(caStack* stack)
{
    Term* t = as_term_ref(circa_input(stack, 0));
    if (t == NULL)
        return circa_output_error(stack, "NULL reference");

    set_int(circa_output(stack, 0), t->id);
}
void Term__properties(caStack* stack)
{
    Term* t = as_term_ref(circa_input(stack, 0));
    if (t == NULL)
        return circa_output_error(stack, "NULL reference");
    circa::copy(&t->properties, circa_output(stack, 0));
}
void Term__property(caStack* stack)
{
    Term* t = as_term_ref(circa_input(stack, 0));
    if (t == NULL)
        return circa_output_error(stack, "NULL reference");

    const char* key = circa_string_input(stack, 1);

    caValue* value = term_get_property(t, key);

    if (value == NULL)
        set_null(circa_output(stack, 0));
    else
        circa::copy(value, circa_output(stack, 0));
}

void is_overloaded_func(caStack* stack)
{
    Block* self = (Block*) circa_block(circa_input(stack, 0));
    set_bool(circa_output(stack, 0), is_overloaded_function(self));
}

void overload__get_contents(caStack* stack)
{
    Block* self = (Block*) circa_block(circa_input(stack, 0));
    caValue* out = circa_output(stack, 0);
    set_list(out, 0);

    list_overload_contents(self, out);
}

void reflection_install_functions(Block* kernel)
{
    static const ImportRecord records[] = {
        {"term_ref", term_ref},
        {"block_ref", block_ref},
        {"Block.dump", Block__dump},
        {"Block.call", Block__call},
        {"Block.find_term", Block__find_term},
        {"Block.file_signature", Block__file_signature},
        {"Block.functions", Block__functions},
        {"Block.statements", Block__statements},
        {"Block.format_source", Block__format_source},
        {"Block.format_function_heading", Block__format_function_heading},
        {"Block.get_term", Block__get_term},
        {"Block.get_static_errors", Block__get_static_errors},
        {"Block.get_static_errors_formatted", Block__get_static_errors_formatted},
        {"Block.has_static_error", Block__has_static_error},
        {"Block.input", Block__input},
        {"Block.inputs", Block__inputs},
        {"Block.is_null", Block__is_null},
        {"Block.link", Block__link},
        {"Block.list_configs", Block__list_configs},
        {"Block.output", Block__output},
        {"Block.outputs", Block__outputs},
        {"Block.owner", Block__owner},
        {"Block.property", Block__property},
        {"Block.properties", Block__properties},
        {"Block.terms", Block__terms},
        {"Block.version", Block__version},
        {"Block.walk_terms", Block__walk_terms},
        {"Term.assign", Term__assign},
        {"Term.asint", Term__asint},
        {"Term.asfloat", Term__asfloat},
        {"Term.format_source", Term__format_source},
        {"Term.format_source_normal", Term__format_source_normal},
        {"Term.function", Term__function},
        {"Term.get_type", Term__type},
        {"Term.tweak", Term__tweak},
        {"Term.input", Term__input},
        {"Term.inputs", Term__inputs},
        {"Term.name", Term__name},
        {"Term.num_inputs", Term__num_inputs},
        {"Term.parent", Term__parent},
        {"Term.contents", Term__contents},
        {"Term.is_null", Term__is_null},
        {"Term.source_location", Term__source_location},
        {"Term.location_string", Term__location_string},
        {"Term.global_id", Term__global_id},
        {"Term.to_string", Term__to_string},
        {"Term.to_source_string", Term__to_source_string},
        {"Term.properties", Term__properties},
        {"Term.property", Term__property},
        {"Term.value", Term__value},

        {"is_overloaded_func", is_overloaded_func},
        {"overload_get_contents", overload__get_contents},
    
        {NULL, NULL}
    };

    install_function_list(kernel, records);
}

} // namespace circa
