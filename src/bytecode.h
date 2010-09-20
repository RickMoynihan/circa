// Copyright (c) 2007-2010 Paul Hodge. All rights reserved.

#pragma once

#include <vector>

#include "common_headers.h"
#include "types/list.h"

namespace circa {
namespace bytecode {

enum OpId {
    OP_CALL = 1,
    OP_PUSH_VALUE,
    OP_PUSH_LOCAL,
    OP_JUMP,
    OP_JUMP_IF,
    OP_JUMP_IF_NOT,
    OP_RETURN,
    OP_PUSH_INT,
    OP_INCREMENT,
    OP_GET_INDEX,
    OP_APPEND,
    OP_NUM_ELEMENTS,
    OP_COPY,
    OP_RAISE,
    OP_CHECK_ERROR,
    OP_COMMENT,
    OP_VAR_NAME
};

struct Operation {
    OpId opid;
    int data[0];
};

struct CallOperation {
    struct Input {
        int registerIndex;
    };

    OpId opid;
    Term* caller;
    Term* function;
    int numInputs;
    int outputIndex;
    Input inputs[0];
};

struct PushValueOperation {
    OpId opid;
    Term* source;
    int outputIndex;
};
struct PushLocalOperation {
    OpId opid;
    int localIndex;
    int output;
};

struct ReturnOperation {
    OpId opid;
    Term* caller;
    int registerIndex;
};

struct JumpOperation {
    OpId opid;
    int offset;
};

struct JumpIfOperation {
    OpId opid;
    int conditionIndex;
    int offset;
};

struct JumpIfNotOperation {
    OpId opid;
    int conditionIndex;
    int offset;
};

struct PushIntOperation {
    OpId opid;
    int value;
    int outputIndex;
};
struct IncrementOperation {
    OpId opid;
    int registerIndex;
};
struct GetIndexOperation {
    OpId opid;
    int listIndex;
    int indexInList;
    int outputIndex;
};
struct AppendOperation {
    OpId opid;
    int itemIndex;
    int outputIndex;
};
struct NumElementsOperation {
    OpId opid;
    int listIndex;
    int outputIndex;
};
struct CopyOperation {
    OpId opid;
    int fromIndex;
    int toIndex;
};
struct RaiseOperation {
    OpId opid;
    int value;
};
struct CheckErrorOperation {
    OpId opid;
};
struct CommentOperation {
    OpId opid;
    int size;
    char text[0];
};
struct VarNameOperation {
    OpId opid;
    int size;
    int registerIndex;
    char name[0];
};

struct BytecodePosition;

struct WriteContext {
    struct PendingStateFieldStore {
        std::string fieldName;
        int nameRegister;
        int resultRegister;
    };

    BytecodeData* bytecode;
    int nextStackIndex;
    size_t sizeWritten;

    // Register index of top level state
    int topLevelState;

    // Register index of state object for the current branch
    int inlineState;

    std::vector<PendingStateFieldStore> pendingStateFieldStores;

    WriteContext(BytecodeData* _bytecode);

    void guaranteeSize(size_t moreBytes);
    void advance(size_t bytes);
    int getOffset();
    Operation* writePos();
    BytecodePosition getPosition();
    int appendLocal(TaggedValue* val);

    // Either fieldName or resultRegister is required; if fieldName is non-blank
    // then resultRegister should be -1.
    void appendStateFieldStore(std::string const& fieldName, int nameRegister,
            int resultRegister);
};

struct BytecodeData {
    bool needsUpdate;
    bool inuse;

    List locals;
    int registerCount;

    size_t size;
    size_t capacity;
    char* opdata;

    BytecodeData()
      : needsUpdate(true),
        inuse(false),
        registerCount(0),
        size(0),
        capacity(0),
        opdata(NULL) {}
};

struct BytecodePosition {
    BytecodeData* data;
    size_t offset;
    BytecodePosition(BytecodeData* d, size_t o) : data(d), offset(o) {}

    Operation* get() { return (Operation*) (data->opdata + offset); }
};

// The size of the operation, in words.
size_t get_operation_size(Operation* op);
bool does_term_have_output(Term* term);
bool should_term_generate_call(Term* term);

void write_call_op(WriteContext* context, Term* caller, Term* function, int numInputs, int* inputIndexes, int outputIndex);
int write_push_local_op(WriteContext* context, TaggedValue* value);
void write_call_op(WriteContext* context, Term* term);
void write_jump(WriteContext* context, int offset);
void write_jump_if(WriteContext* context, int conditionIndex, int offset);
void write_jump_if_not(WriteContext* context, int conditionIndex, int offset);
void write_push_int(WriteContext* context, int value, int registerIndex);
void write_get_index(WriteContext* context, int listIndex, int indexInList, int outputIndex);
void write_increment(WriteContext* context, int intIndex);
void write_num_elements(WriteContext* context, int listIndex, int outputIndex);
void write_copy(WriteContext* context, int fromIndex, int toIndex);
void write_raise(WriteContext* context);
void write_comment(WriteContext* context, const char* str);
void write_var_name(WriteContext* context, int registerIndex, const char* name);
void write_op(WriteContext* context, Term* term);

// Give this term the next available register index, if it doesn't already have one.
void assign_register_index(WriteContext* context, Term* term);

// Writes operations inside the given branch. If there are any state vars, we'll pull
// them out of the container with stack index 'inlineState'. Returns the stack index of
// the last expression (this is sometimes used as a return value).
int write_bytecode_for_branch(WriteContext* context, Branch& branch, int inlineState);

void write_bytecode_for_branch_inline(WriteContext* context, Branch& branch);
void write_raise_if(WriteContext* context, Term* errorCondition);
void write_get_state_field(WriteContext* context, Term* term, int name,
        int defaultValue, int output);

void resize_opdata(BytecodeData* bytecode, size_t newCapacity);
void update_bytecode(Branch& branch, BytecodeData* bytecode);
void update_bytecode(Branch& branch);

void print_bytecode(std::ostream& out, BytecodeData* data);
void print_bytecode(std::ostream& out, Branch& branch);
void print_operation(std::ostream& out, BytecodeData* bytecode, Operation* op);
void print_bytecode_raw(std::ostream& out, BytecodeData* data);
void print_bytecode_for_all_major_branches(std::ostream& out, Branch& branch);

CallOperation* create_orphan_call_operation(Term* caller, Term* function, int numInputs);
void free_orphan_call_operation(CallOperation* op);

void evaluate_bytecode(EvalContext* cxt, BytecodeData* data, List* stack);

struct Iterator {
    Operation* pos;
    Operation* end;

    Iterator(BytecodeData* data)
      : pos((Operation*) data->opdata),
        end((Operation*) (data->opdata + data->size)) {}

    bool finished();
    Operation* current();
    void advance();
    Operation* operator*() { return current(); }
    Operation* operator->() { return current(); }
    void operator++() { advance(); }
};

} // namespace bytecode
} // namespace circa
