// Copyright (c) Paul Hodge. See LICENSE file for license terms.

#include "../branch.h"
#include "../common_headers.h"
#include "../evaluation.h"
#include "../list_shared.h"
#include "../modules.h"
#include "../source_repro.h"
#include "../static_checking.h"
#include "../string_type.h"
#include "../names.h"
#include "../tagged_value.h"
#include "../token.h"
#include "../types/list.h"

namespace circa {

void read_stdin_line(TValue* line)
{
    char* buf = NULL;
    size_t size = 0;
    ssize_t read = getline(&buf, &size, stdin);

    if (read == -1) {
        set_null(line);
        free(buf);
        return;
    }

    buf[read] = 0;

    // Truncate newline
    if (read > 0 && buf[read-1] == '\n')
        buf[read - 1] = 0;

    set_string(line, buf);
    free(buf);
}

void parse_tokens_as_argument_list(TokenStream* tokens, List* output)
{
    // Read the tokens as a space-seperated list of strings.
    // TODO is to be more smart about word boundaries: spaces inside
    // quotes or parentheses shouldn't break apart items.
    
    TValue itemInProgress;
    set_string(&itemInProgress, "");

    while (!tokens->finished()) {

        if (tokens->nextIs(TK_WHITESPACE)) {
            if (!equals_string(&itemInProgress, "")) {
                copy(&itemInProgress, list_append(output));
                set_string(&itemInProgress, "");
            }

        } else {
            string_append(&itemInProgress, tokens->nextStr().c_str());
        }

        tokens->consume();
    }

    if (!equals_string(&itemInProgress, "")) {
        copy(&itemInProgress, list_append(output));
        set_string(&itemInProgress, "");
    }
}

void do_add_lib_path(List* args, TValue* reply)
{
    modules_add_search_path(as_cstring(list_get(args, 0)));
}

void do_echo(List* args, TValue* reply)
{
    set_string(reply, to_string(args));
}

void do_file_command(List* args, TValue* reply)
{
    bool printRaw = false;
    bool printRawWithProps = false;
    bool printSource = false;
    bool printState = false;
    bool dontRunScript = false;

    int argIndex = 0;

    while (true) {

        if (argIndex >= args->length()) {
            set_string(reply, "No filename found");
            return;
        }

        if (string_eq(args->get(argIndex), "-p")) {
            printRaw = true;
            argIndex++;
            continue;
        }

        if (string_eq(args->get(argIndex), "-pp")) {
            printRawWithProps = true;
            argIndex++;
            continue;
        }

        if (string_eq(args->get(argIndex), "-s")) {
            printSource = true;
            argIndex++;
            continue;
        }
        if (string_eq(args->get(argIndex), "-print-state")) {
            printState = true;
            argIndex++;
            continue;
        }
        if (string_eq(args->get(argIndex), "-n")) {
            dontRunScript = true;
            argIndex++;
            continue;
        }

        break;
    }

    Branch branch;
    load_script(&branch, as_cstring(args->get(argIndex)));

    if (printSource)
        std::cout << get_branch_source_text(&branch);

    if (has_static_errors(&branch)) {
        print_static_errors_formatted(&branch, reply);
        return;
    }

    if (dontRunScript)
        return;
    
    EvalContext context;
    evaluate_branch(&context, &branch);

    if (printState)
        std::cout << context.state.toString() << std::endl;

    if (error_occurred(&context)) {
        std::cout << "Error occurred:\n";
        print_runtime_error_formatted(&context, std::cout);
        std::cout << std::endl;
        return;
    }
}

void do_command(TValue* string, TValue* reply)
{
    // Tokenize the incoming string
    TokenStream tokens;
    tokens.reset(string);

    if (tokens.length() == 0) {
        set_null(reply);
        return;
    }

    TValue command;
    tokens.consumeStr(&command);

    set_null(reply);

    if (equals_string(&command, "add_lib_path")) {
        List args;
        parse_tokens_as_argument_list(&tokens, &args);

    } else if (equals_string(&command, "file")) {

        List args;
        parse_tokens_as_argument_list(&tokens, &args);
        do_file_command(&args, reply);

    } else if (equals_string(&command, "echo")) {

        List args;
        parse_tokens_as_argument_list(&tokens, &args);
        do_echo(&args, reply);

    } else {

        set_string(reply, "Unrecognized command: ");
        string_append(reply, &command);
    }
}

void run_commands_from_stdin()
{
    while (true) {
        TValue line;
        read_stdin_line(&line);
        if (!is_string(&line))
            break;

        TValue reply;
        do_command(&line, &reply);

        if (is_null(&reply))
            ; // no op
        else if (is_string(&reply))
            std::cout << as_string(&reply) << std::endl;
        else
            std::cout << to_string(&reply) << std::endl;

        // We need to tell the stdout reader that we have finished. The proper
        // way to do this would probably be to format the entire output as an
        // escapted string. But, this is what we'll do for now:
        std::cout << ":done" << std::endl;
    }
}

} // namespace circa
