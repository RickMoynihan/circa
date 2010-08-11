// Copyright (c) 2007-2010 Paul Hodge. All rights reserved.

#include "common_headers.h"

#include "circa.h"

namespace circa {

// The format of if_block is as follows:
//
// N = branch length
//
// {
//   [0] if(cond0) : Branch
//   [1] if(cond1) : Branch
//   ...
//   [N-2] branch()  (this corresponds to the 'else' block)
//   [N-1] #joining = branch() 
//
//
// Bytecode:
//
// if <expr0>
//   ..branch A..
//   name = x
// elif <expr1>
//   ..branch B..
//   name = y
// else
//   ..branch Z..
// end
// name = join()
//
// 0: expr0
// 1: jump_if_not(0) to 2
// .. branch A
// .. jump to End
// 2: expr1
//    jump_if_not(2) to
// .. branch B
// ..
// End
//
// Algorithm:
//   Figure out stack positions for exported names

void update_if_block_joining_branch(Term* ifCall)
{
    Branch& contents = ifCall->nestedContents;

    // Create the joining contents if necessary
    if (!contents.contains("#joining"))
        create_branch(contents, "#joining");

    Branch& joining = contents["#joining"]->nestedContents;
    joining.clear();

    // This is used later.
#ifndef BYTECODE
    Term* satisfiedIndex = create_int(joining, 0, "#satisfiedIndex");
#endif

    // Find the set of all names bound in every branch.
    std::set<std::string> boundNames;

    {
        for (int branch_index=0; branch_index < contents.length()-1; branch_index++) {
            Term* term = contents[branch_index];
            Branch& branch = is_branch(term) ? as_branch(term) : term->nestedContents;

            TermNamespace::const_iterator it;
            for (it = branch.names.begin(); it != branch.names.end(); ++it)
                boundNames.insert(it->first);
        }
    }

    Branch* outerScope = ifCall->owningBranch;
    ca_assert(outerScope != NULL);

    // Filter out some names from boundNames.
    for (std::set<std::string>::iterator it = boundNames.begin(); it != boundNames.end();)
    {
        std::string const& name = *it;

        // Ignore hidden names
        if (name[0] == '#') {
            boundNames.erase(it++);
            continue;
        }

        // We only rebind names that are either 1) already bound in the outer scope, or
        // 2) bound in every possible branch.
        
        bool boundInOuterScope = find_named(*outerScope, name) != NULL;

        int numberOfBranchesWithThisName = 0;
        bool boundInEveryBranch = true;

        for (int branch_index=0; branch_index < contents.length()-1; branch_index++) {
            Term* term = contents[branch_index];
            Branch& branch = is_branch(term) ? as_branch(term) : term->nestedContents;
            if (branch.contains(name))
                numberOfBranchesWithThisName++;
            else
                boundInEveryBranch = false;
        }

        if (!boundInOuterScope && !boundInEveryBranch)
            boundNames.erase(it++);
        else
            ++it;
    }

    // For each name, create a term that selects the correct version of this name.
    for (std::set<std::string>::const_iterator it = boundNames.begin();
            it != boundNames.end();
            ++it)
    {
        std::string const& name = *it;

#ifdef BYTECODE
        apply(joining, JOIN_FUNC, RefList(), name);
#else

        // Make a list where we find the corresponding term for this name in each branch.
        Term* selections = apply(joining, BRANCH_FUNC, RefList());
        Branch& selectionsBranch = selections->nestedContents;

        for (int branch_index=0; branch_index < contents.length()-1; branch_index++) {
            Term* term = contents[branch_index];
            Branch& branch = is_branch(term) ? as_branch(term) : term->nestedContents;

            Term* selection = NULL;
            if (branch.contains(name))
                selection = branch[name];
            else
                selection = find_named(*outerScope, name);

            apply(selectionsBranch, COPY_FUNC, RefList(selection));
        }

        apply(joining, GET_INDEX_FROM_BRANCH_FUNC, RefList(selections, satisfiedIndex), name);
#endif
    }

    // Expose all names in 'joining' branch.
    expose_all_names(joining, *outerScope);
}

Branch* get_if_condition_block(Term* ifCall, int index)
{
    ca_assert(ifCall->function = IF_BLOCK_FUNC);
    Branch& callContents = ifCall->nestedContents;
    ca_assert(index < callContents.length());
    return &(callContents[index]->nestedContents);
}

Branch* get_if_block_else_block(Term* ifCall)
{
    ca_assert(ifCall->function = IF_BLOCK_FUNC);
    Branch& callContents = ifCall->nestedContents;
    ca_assert(callContents.length() >= 2);
    return &(callContents[callContents.length()-2]->nestedContents);
}

List* get_if_block_state(Term* ifCall)
{
    Term* term = ifCall->input(0);
    if (term == NULL)
        return NULL;
    return List::checkCast(term);
}

bool if_block_contains_state(Term* ifCall)
{
    Branch& contents = ifCall->nestedContents;
    for (int cond=0; cond < contents.length(); cond++) {
        Branch& condContents = contents[cond]->nestedContents;
        for (int i=0; i < condContents.length(); i++) {
            if (is_stateful(condContents[i]))
                return true;
        }
    }
    return false;
}

void evaluate_if_block(EvalContext* cxt, Term* caller)
{
    Branch& contents = caller->nestedContents;
    List* state = get_if_block_state(caller);

    if (state != NULL) {
        int numElements = contents.length() - 1;
        int actualElements = state->numElements();
        state->resize(numElements);
        for (int i=actualElements; i < numElements; i++)
            make_list(state->get(i));
    }

    // Find the first if() call whose condition is true
    int satisfiedIndex = 0;
    for (int i=0; i < contents.length()-1; i++) {
        Term* call = contents[i];

        TaggedValue* stateElement = NULL;
        if (state != NULL)
            stateElement = state->get(i);

        bool satisfied = false;

        if (call->function == BRANCH_FUNC) {
            satisfied = true;
        } else {
            Term* cond = call->input(0);
            if (cond->asBool())
                satisfied = true;
        }

        if (satisfied) {
            // Load state, if it's found
            if (stateElement != NULL) {
                load_state_into_branch(stateElement, call->nestedContents);
            }

            evaluate_term(cxt, call);
            satisfiedIndex = i;

            if (stateElement != NULL) {
                // State elements may have moved during evaluate_term,
                // so call state->get again.
                stateElement = state->get(i);
                persist_state_from_branch(call->nestedContents, stateElement);
            }

            break;

        } else {
            // This condition wasn't executed, reset state.
            if (stateElement != NULL) {
                reset(stateElement);
                reset_state(call->nestedContents);
            }
        }
    }

    // For any conditions after the successful one, reset state.
    for (int i=satisfiedIndex+1; i < contents.length()-1; i++) {
        TaggedValue* stateElement = NULL;
        if (state != NULL) {
            stateElement = state->get(i);
        }
        if (stateElement != NULL) {
            reset(stateElement);
            Term* call = contents[i];
            reset_state(call->nestedContents);
        }
    }

    // Update the #joining branch
    ca_assert(contents[contents.length()-1]->name == "#joining");
    Branch& joining = contents[contents.length()-1]->nestedContents;
    set_int(joining["#satisfiedIndex"], satisfiedIndex);
    evaluate_branch(cxt, joining);
}

void write_if_block_bytecode(bytecode::WriteContext* context, Term* ifBlock)
{
    Branch& blockContents = ifBlock->nestedContents;
    Branch& joining = blockContents["#joining"]->nestedContents;

    int numBranches = blockContents.length() - 1;
    bool assignStackIndexes = context->writePos != NULL;

    if (assignStackIndexes) {
        // For each name in #joining, we want corresponding variables (across the
        // branches in this if-block) to have the same stack indexes.
        for (int i=0; i < joining.length(); i++) {
            if (joining[i]->stackIndex == -1)
                joining[i]->stackIndex = context->nextStackIndex++;
            std::string& name = joining[i]->name;
            ca_assert(name != "");

            // Find the corresponding term in each branch, give it this stack index.
            for (int b=0; b < numBranches; b++) {
                Term* term = blockContents[b]->nestedContents[name];
                term->stackIndex = joining[i]->stackIndex;
            }
        }
    }
    
    // Go through each branch
    bytecode::JumpIfNotOperation *lastBranchJumpOp = NULL;
    std::vector<bytecode::JumpOperation*> jumpsToEnd;
    for (int branch=0; branch < numBranches; branch++) {
        Term* term = blockContents[branch];

        // Check if we need to write the address for the previous branch's jump
        if (lastBranchJumpOp) {
            lastBranchJumpOp->offset = context->getOffset();
            lastBranchJumpOp = NULL;
        }

        // Jump check, for 'if' and 'elsif'. Don't know offset yet, will write it later.
        if (term->function == IF_FUNC) {
            lastBranchJumpOp = (bytecode::JumpIfNotOperation*) context->writePos;
            bytecode::write_jump_if_not(context, term->input(0)->stackIndex, 0);
        }

        // Write each instruction in this branch
        Branch& branchContents = term->nestedContents;
        for (int i=0; i < branchContents.length(); i++) {
            bytecode::write_op(context, branchContents[i]);
        }
        
        // Jump past remaining branches. But, don't need to do this for last branch.
        if (branch+1 < numBranches) {
            if (context->writePos)
                jumpsToEnd.push_back((bytecode::JumpOperation*) context->writePos);
            bytecode::write_jump(context, 0);
        }
    }

    if (context->writePos) {
        for (size_t i=0; i < jumpsToEnd.size(); i++)
            jumpsToEnd[i]->offset = context->getOffset();
    }
}

} // namespace circa
