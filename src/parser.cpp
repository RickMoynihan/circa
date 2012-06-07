// Copyright (c) Andrew Fischer. See LICENSE file for license terms.

#include "common_headers.h"

#include "branch.h"
#include "building.h"
#include "evaluation.h"
#include "loops.h"
#include "function.h"
#include "if_block.h"
#include "handle.h"
#include "introspection.h"
#include "list.h"
#include "kernel.h"
#include "modules.h"
#include "parser.h"
#include "source_repro.h"
#include "stateful_code.h"
#include "static_checking.h"
#include "string_type.h"
#include "switch_block.h"
#include "names.h"
#include "term.h"
#include "token.h"
#include "type.h"

namespace circa {
namespace parser {

Term* compile(Branch* branch, ParsingStep step, std::string const& input)
{
    log_start(0, "parser::compile");
    log_arg("branch.id", branch->id);
    log_arg("input", input.c_str());
    log_finish();

    ca_assert(input != NULL);

    branch_start_changes(branch);

    TokenStream tokens(input);
    ParserCxt context;
    Term* result = step(branch, tokens, &context).term;

    branch_finish_changes(branch);

    ca_assert(branch_check_invariants_print_result(branch, std::cout));

    log_start(0, "parser::compile finished");
    log_arg("branch.length", branch->length());
    log_finish();

    return result;
}

Term* evaluate(Branch* branch, ParsingStep step, std::string const& input)
{
    int prevHead = branch->length();

    Term* result = compile(branch, step, input);

    Stack context;

    evaluate_range(&context, branch, prevHead, branch->length() - 1);

    return result;
}

// -------------------------- Utility functions -------------------------------

// This structure stores the syntax hints for list-like syntax. It exists because
// you usually don't have a comprehension term while you are parsing the list
// arguments, so you need to temporarily store syntax hints until you create one.
struct ListSyntaxHints {
    List inputs;

    void insert(int index)
    {
        set_dict(inputs.insert(index));
    }

    void set(int index, std::string const& field, std::string const& value)
    {
        while (index >= inputs.length())
            set_dict(inputs.append());

        Dict* dict = as_dict(inputs[index]);
        set_string(dict->insert(field.c_str()), value.c_str());
    }
    void set(int index, const char* field, caValue* value)
    {
        while (index >= inputs.length())
            set_dict(inputs.append());

        Dict* dict = as_dict(inputs[index]);
        copy(value, dict->insert(field));
    }

    void append(int index, std::string const& field, std::string const& value)
    {
        while (index >= inputs.length())
            set_dict(inputs.append());

        Dict* dict = as_dict(inputs[index]);

        caValue* existing = dict->insert(field.c_str());
        if (!is_string(existing))
            set_string(existing, "");

        set_string(existing, as_string(existing) + value);
    }

    void apply(Term* term)
    {
        for (int i=0; i < inputs.length(); i++) {
            Dict* dict = as_dict(inputs[i]);
            Value it;
            for (dict->iteratorStart(&it);
                    !dict->iteratorFinished(&it); dict->iteratorNext(&it)) {
                const char* key;
                caValue* value;
                dict->iteratorGet(&it, &key, &value);

                set_input_syntax_hint(term, i, key, value);
            }
        }
    }
};

void consume_branch(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    Term* parentTerm = branch->owningTerm;

    if (tokens.nextNonWhitespaceIs(tok_LBrace)) {
        consume_branch_with_braces(branch, tokens, context, parentTerm);
    } else {
        consume_branch_with_significant_indentation(branch, tokens, context, parentTerm);
    }

    return;
}

int find_indentation_of_next_statement(TokenStream& tokens)
{
    // Lookahead and find the next non-whitespace statement.
    int lookahead = 0;
    while (!tokens.finished()) {
        if (tokens.nextIs(tok_Whitespace, lookahead))
            lookahead++;
        else if (tokens.nextIs(tok_Newline, lookahead))
            lookahead++;
        else
            break;
    }

    if (lookahead >= tokens.remaining())
        return -1;

    return tokens.next(lookahead).colStart;
}

void consume_branch_with_significant_indentation(Branch* branch, TokenStream& tokens,
        ParserCxt* context, Term* parentTerm)
{
    ca_assert(parentTerm != NULL);
    ca_assert(parentTerm->sourceLoc.defined());

    parentTerm->setStringProp("syntax:branchStyle", "sigIndent");

    int parentTermIndent = tokens.next(-1).precedingIndent;

    // Consume the whitespace immediately after the heading (and possibly a newline).
    std::string postHeadingWs = possible_statement_ending(tokens);
    bool foundNewline = postHeadingWs.find_first_of("\n") != std::string::npos;

    parentTerm->setStringProp("syntax:postHeadingWs", postHeadingWs);

    // If we're still on the same line, keep consuming statements. We might only
    // find a comment (in which case we should keep parsing subsequent lines),
    // or we might find statements (making this a one-liner).
    bool foundStatementOnSameLine = false;

    if (!foundNewline) {
        while (!tokens.finished()) {

            // Special case for if-blocks. If we hit an if-block seperator then finish,
            // but don't consume it.
            if (tokens.nextIs(tok_Else) || tokens.nextIs(tok_Elif))
                return;

            Term* statement = parser::statement(branch, tokens, context).term;

            std::string const& lineEnding = statement->stringProp("syntax:lineEnding");
            bool hasNewline = lineEnding.find_first_of("\n") != std::string::npos;

            if (statement->function != FUNCS.comment)
                foundStatementOnSameLine = true;

            // If we hit a newline then stop parsing
            if (hasNewline) {

                // If we hit any statements on this line, then stop parsing here.
                // Example:
                //    def f() return 1 + 2
                // Also, steal this term's lineEnding and use it as the parent term's line
                // ending.
                if (foundStatementOnSameLine) {

                    parentTerm->setStringProp("syntax:lineEnding",
                            statement->stringProp("syntax:lineEnding"));
                    statement->removeProperty("syntax:lineEnding");
                }
                return;
            }
        }
    }

    // Lookahead and find the next non-whitespace statement.
    int lookahead = 0;
    while (!tokens.finished()) {
        if (tokens.nextIs(tok_Whitespace, lookahead))
            lookahead++;
        else if (tokens.nextIs(tok_Newline, lookahead))
            lookahead++;
        else
            break;
    }

    // Check if the next statement has an indentation level that is higher
    // or equal to the parent indentation. If so then stop and don't consume
    // any more.

    if (find_indentation_of_next_statement(tokens) <= parentTermIndent) {
        // Take the line ending that we parsed as postHeadingWs, and move it over
        // to lineEnding instead (so that we don't parse another line ending).
        parentTerm->setStringProp("syntax:lineEnding",
                parentTerm->stringProp("syntax:postHeadingWs"));
        parentTerm->removeProperty("syntax:postHeadingWs");
        return;
    }

    // At this point we're ready to parse some statements. The first statement
    // will tell us the indentation level for the whole block. But, we'll ignore
    // comments when figuring this out. Example:
    //     def f()
    //
    //             -- a misplaced comment
    //         a = 1
    //         return a + 2

    parentTerm->setBoolProp("syntax:multiline", true);

    int indentationLevel = 0;
    while (!tokens.finished()) {

        // Don't consume if the next identation is less than or equal to our starting
        // indent. We can get into this situation by the following code fragment:
        // 
        //     for i in []
        //         -- a comment
        //     next_line
        //
        if (find_indentation_of_next_statement(tokens) <= parentTermIndent)
            return;
        
        Term* statement = parser::statement(branch, tokens, context).term;

        if (statement->function != FUNCS.comment) {
            indentationLevel = int(statement->stringPropOptional(
                "syntax:preWhitespace", "").length());
            break;
        }
    }

    // Now keep parsing lines which have the same indentation level
    while (!tokens.finished()) {

        // Lookahead, check if the next line has the same indentation

        int nextIndent = 0;
        if (tokens.nextIs(tok_Whitespace))
            nextIndent = (int) tokens.next().length();

        // Check if the next line is just a comment/whitespace
        bool ignore = lookahead_match_whitespace_statement(tokens)
            || lookahead_match_comment_statement(tokens);

        if (!ignore && (indentationLevel != nextIndent))
            break;

        parser::statement(branch, tokens, context);
    }
}

void consume_branch_with_braces(Branch* branch, TokenStream& tokens, ParserCxt* context,
        Term* parentTerm)
{
    parentTerm->setStringProp("syntax:branchStyle", "braces");

    if (tokens.nextIs(tok_Whitespace))
        parentTerm->setStringProp("syntax:postHeadingWs", possible_whitespace(tokens));

    tokens.consume(tok_LBrace);

    while (!tokens.finished()) {
        if (tokens.nextIs(tok_RBrace)) {
            tokens.consume();
            return;
        }

        parser::statement(branch, tokens, context);
    }
}

// ---------------------------- Parsing steps ---------------------------------

ParseResult statement_list(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    ParseResult result;

    while (!tokens.finished())
        result = statement(branch, tokens, context);

    return result;
}

ParseResult statement(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int initialPosition = tokens.getPosition();
    std::string preWhitespace = possible_whitespace(tokens);

    int sourceStartPosition = tokens.getPosition();
    bool foundWhitespace = preWhitespace != "";

    ParseResult result;

    // Comment (blank lines count as comments). This should do the same thing
    // as matches_comment_statement.
    if (tokens.nextIs(tok_Comment) || tokens.nextIs(tok_Newline) || tokens.nextIs(tok_Semicolon)
        || (foundWhitespace && (tokens.nextIs(tok_RBrace) || tokens.nextIs(tok_End) || tokens.finished()))) {
        result = comment(branch, tokens, context);
    }

    // Function decl
    else if (tokens.nextIs(tok_Def)) {
        result = function_decl(branch, tokens, context);
    }

    // Type decl
    else if (tokens.nextIs(tok_Type)) {
        result = type_decl(branch, tokens, context);
    }

    // Do_once block
    else if (tokens.nextIs(tok_DoOnce)) {
        result = do_once_block(branch, tokens, context);
    }

    // Stateful value decl
    else if (tokens.nextIs(tok_State)) {
        result = stateful_value_decl(branch, tokens, context);
    }
    // Return statement
    else if (tokens.nextIs(tok_Return)) {
        result = return_statement(branch, tokens, context);
    }

    // Discard statement
    else if (tokens.nextIs(tok_Discard)) {
        result = discard_statement(branch, tokens, context);
    }
    // Break statement
    else if (tokens.nextIs(tok_Break)) {
        result = break_statement(branch, tokens, context);
    }
    // Continue statement
    else if (tokens.nextIs(tok_Continue)) {
        result = continue_statement(branch, tokens, context);
    }

    // Namespace block
    else if (tokens.nextIs(tok_Namespace)) {
        result = namespace_block(branch, tokens, context);
    }

    // Case statement
    else if (tokens.nextIs(tok_Case)) {
        result = case_statement(branch, tokens, context);
    }

    // Import statement
    else if (tokens.nextIs(tok_Import)) {
        result = import_statement(branch, tokens, context);
    }

    // Otherwise, expression statement
    else {
        result = expression_statement(branch, tokens, context);
    }

    prepend_whitespace(result.term, preWhitespace);

    set_source_location(result.term, sourceStartPosition, tokens);

    if (!is_multiline_block(result.term) && !result.term->hasProperty("syntax:lineEnding")) {

        // Consume some trailing whitespace
        append_whitespace(result.term, possible_whitespace(tokens));

        // Consume a newline or ; or ,
        result.term->setStringProp("syntax:lineEnding", possible_statement_ending(tokens));
    }

    // Mark this term as a statement
    set_is_statement(result.term, true);

    // Avoid an infinite loop
    if (initialPosition == tokens.getPosition())
        internal_error("parser::statement is stuck, next token is: " + tokens.nextStr());

    return result;
}

bool matches_comment_statement(Branch* branch, TokenStream& tokens)
{
    int lookahead = 0;
    bool foundWhitespace = false;
    if (tokens.nextIs(tok_Whitespace, lookahead)) {
        lookahead++;
        foundWhitespace = true;
    }

    int next = tokens.next(lookahead).match;
    return (next == tok_Comment || next == tok_Newline || next == tok_Semicolon ||
        (foundWhitespace &&
             (tokens.nextIs(tok_RBrace) || tokens.nextIs(tok_End) || tokens.finished())));
}

ParseResult comment(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    std::string commentText;

    if (tokens.nextIs(tok_Comment)) {
        commentText = tokens.nextStr();
        tokens.consume();
    }

    Term* result = apply(branch, FUNCS.comment, TermList());
    result->setStringProp("comment", commentText);

    return ParseResult(result);
}

ParseResult type_expr(Branch* branch, TokenStream& tokens,
        ParserCxt* context)
{
    int startPosition = tokens.getPosition();

    ParseResult result;

    if (tokens.nextIs(tok_LBracket)) {
        result = anonymous_type_decl(branch, tokens, context);
        if (has_static_error(result.term))
            return compile_error_for_line(result.term, tokens, startPosition);

    } else {
        if (!tokens.nextIs(tok_Identifier))
            return compile_error_for_line(branch, tokens, startPosition);

        std::string typeName = tokens.consumeStr();

        Term* typeTerm = find_name(branch, typeName.c_str(), -1, NAME_LOOKUP_TYPE);

        if (typeTerm == NULL) {
            // TODO: This name lookup failure should be recorded.
            typeTerm = ANY_TYPE;
        }

        result = ParseResult(typeTerm, typeName);
    }

    return result;
}

bool token_is_allowed_as_function_name(int token)
{
    switch (token) {
        case tok_For: case tok_If: case tok_Include: case tok_Type: case tok_Not:
            return true;
        default:
            return false;
    }
}

ParseResult function_decl(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();

    if (tokens.nextIs(tok_Def))
        tokens.consume(tok_Def);

    possible_whitespace(tokens);

    if (tokens.finished()
            || (!tokens.nextIs(tok_Identifier)
            && !token_is_allowed_as_function_name(tokens.next().match))) {
        return compile_error_for_line(branch, tokens, startPosition, "Expected identifier");
    }

    // Function name
    Value functionName;
    tokens.consumeStr(&functionName, tok_Identifier);

    bool isMethod = false;
    Term* methodType = NULL;

    // Check if this is a method declaration (declared as Typename.funcname)
    if (tokens.nextIs(tok_Dot)) {
        isMethod = true;

        tokens.consume(tok_Dot);

        if (!tokens.nextIs(tok_Identifier))
            return compile_error_for_line(branch, tokens, startPosition, "Expected identifier after .");

        Value typeName;
        copy(&functionName, &typeName);
        methodType = find_name(branch, as_cstring(&typeName));
        string_append(&functionName, ".");
        string_append(&functionName, tokens.consumeStr(tok_Identifier).c_str());

        if (methodType == NULL || !is_type(methodType))
            return compile_error_for_line(branch, tokens, startPosition,
                      "Not a type: " + as_string(&typeName));
    }

    Term* result = create_function(branch, as_cstring(&functionName));

    Function* attrs = as_function(result);
    set_starting_source_location(result, startPosition, tokens);
    attrs->name = as_string(&functionName);

    if (methodType != NULL)
        result->setBoolProp("syntax:methodDecl", true);

    result->setStringProp("syntax:postNameWs", possible_whitespace(tokens));

    // Optional list of qualifiers
    while (tokens.nextIs(tok_Name)) {
        std::string symbolText = tokens.consumeStr(tok_Name);
        if (symbolText == ":throws")
            attrs->throws = true;
        else
            return compile_error_for_line(branch, tokens, startPosition,
                    "Unrecognized symbol: "+symbolText);

        symbolText += possible_whitespace(tokens);

        result->setStringProp("syntax:properties", result->stringProp("syntax:properties")
                + symbolText);
    }

    if (!tokens.nextIs(tok_LParen))
        return compile_error_for_line(branch, tokens, startPosition, "Expected: (");

    tokens.consume(tok_LParen);

    Branch* contents = nested_contents(result);

    int qualifierLoc = find_qualified_name_separator(as_cstring(&functionName));
    if (qualifierLoc != -1)
        return compile_error_for_line(branch, tokens, startPosition,
                "Can't declare function with qualified name: " + as_string(&functionName));

    // Input arguments
    int inputIndex = 0;
    while (!tokens.nextIs(tok_RParen) && !tokens.finished())
    {
        bool isStateArgument = false;

        possible_whitespace(tokens);

        // check for 'state' keyword
        if (tokens.nextIs(tok_State)) {
            tokens.consume(tok_State);
            possible_whitespace(tokens);
            isStateArgument = true;
        }

        Term* typeTerm = NULL;
        if (inputIndex == 0 && isMethod) {
            // For input0 of a method, don't expect an explicit type name.
            typeTerm = methodType;
        } else {
            typeTerm = type_expr(branch, tokens, context).term;
        }

        possible_whitespace(tokens);

        // Optional @, indicating an output.
        bool rebindSymbol = false;
        if (tokens.nextIs(tok_At)) {
            tokens.consume(tok_At);
            rebindSymbol = true;
        }

        std::string name;
        if (tokens.nextIs(tok_Identifier)) {
            name = tokens.consumeStr();
            possible_whitespace(tokens);
        } else {
            // anonymous input; use a default name
            name = get_placeholder_name_for_index(function_num_inputs(attrs));
        }

        // Create an input placeholder term
        Term* input = apply(contents, FUNCS.input, TermList(), name);

        if (is_type(typeTerm))
            change_declared_type(input, as_type(typeTerm));

        if (isStateArgument)
            input->setBoolProp("state", true);

        if (rebindSymbol) {
            input->setBoolProp("output", true);
            input->setBoolProp("syntax:rebindSymbol", true);
        }

        // Variable args when ... is appended
        if (tokens.nextIs(tok_Ellipsis)) {
            std::cout << "ellipsis used in " << as_cstring(&functionName) << std::endl;
            tokens.consume(tok_Ellipsis);
            input->setBoolProp("multiple", true);
            input->flags |= TERM_FLAG_MULTIPLE;
        }

        // Optional list of qualifiers
        while (tokens.nextIs(tok_Name)) {
            std::string symbolText = tokens.consumeStr(tok_Name);

            // TODO: store syntax hint
            if (symbolText == ":ignore_error") {
                input->setBoolProp("ignore_error", true);
            } else if (symbolText == ":optional") {
                input->setBoolProp("optional", true);
            } else if (symbolText == ":output" || symbolText == ":out") {
                input->setBoolProp("output", true);
            } else if (symbolText == ":multiple") {
                input->setBoolProp("multiple", true);
                input->flags |= TERM_FLAG_MULTIPLE;
            } else if (symbolText == ":meta") {
                input->setBoolProp("meta", true);
            } else {
                return compile_error_for_line(branch, tokens, startPosition,
                    "Unrecognized qualifier: "+symbolText);
            }
            possible_whitespace(tokens);
        }

        if (!tokens.nextIs(tok_RParen)) {
            if (!tokens.nextIs(tok_Comma))
                return compile_error_for_line(result, tokens, startPosition, "Expected: ,");

            tokens.consume(tok_Comma);
        }

        inputIndex++;

    } // Done consuming input arguments

    for (int i=0; i < contents->length(); i++)
        hide_from_source(contents->get(i));

    if (!tokens.nextIs(tok_RParen))
        return compile_error_for_line(result, tokens, startPosition);
    tokens.consume(tok_RParen);

    // Another optional list of symbols
    if (tokens.nextNonWhitespaceIs(tok_Name)) {
        possible_whitespace(tokens);
        std::string symbolText = tokens.consumeStr(tok_Name);
        #if 0 // there was once stuff here
        if (symbolText == ":xyz") {
        }
        else
        #endif
        {
            return compile_error_for_line(result, tokens, startPosition,
                "Unrecognized symbol: " + symbolText);
        }
    }

    // Output type
    Term* outputType = VOID_TYPE;

    if (tokens.nextNonWhitespaceIs(tok_RightArrow)) {
        result->setStringProp("syntax:whitespacePreColon", possible_whitespace(tokens));
        tokens.consume(tok_RightArrow);
        result->setStringProp("syntax:whitespacePostColon", possible_whitespace(tokens));

        outputType = type_expr(branch, tokens, context).term;
        ca_assert(outputType != NULL);
    }

    if (!is_type(outputType))
        return compile_error_for_line(result, tokens, startPosition,
                outputType->name +" is not a type");

    ca_assert(is_value(result));
    ca_assert(is_function(result));

    // Create the primary output placeholder
    Term* primaryOutput = append_output_placeholder(contents, NULL);
    change_declared_type(primaryOutput, as_type(outputType));

    // If we're out of tokens, then stop here. This behavior is used when declaring builtins.
    if (tokens.finished()) {
        finish_building_function(as_function(result));
        return ParseResult(result);
    }

    // If we reach this point then the function will be a subroutine.

    // Parse subroutine contents.
    consume_branch(contents, tokens, context);

    finish_building_function(as_function(result));

    ca_assert(is_value(result));
    ca_assert(is_function(result));

    set_source_location(result, startPosition, tokens);

    return ParseResult(result);
}

ParseResult type_decl(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();

    if (tokens.nextIs(tok_Type))
        tokens.consume();

    possible_whitespace(tokens);

    if (!tokens.nextIs(tok_Identifier))
        return compile_error_for_line(branch, tokens, startPosition);

    std::string name = tokens.consumeStr(tok_Identifier);

    Term* result = anonymous_type_decl(branch, tokens, context).term;

    if (has_static_error(result))
        return ParseResult(result);

    rename(result, name);
    as_type(result)->name = name_from_string(name.c_str());

    return ParseResult(result);
}

ParseResult anonymous_type_decl(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();

    Term* result = create_value(branch, &TYPE_T);
    as_type(result)->declaringTerm = result;

    // Attributes
    result->setStringProp("syntax:preLBracketWhitespace",
            possible_whitespace_or_newline(tokens));

    while (tokens.nextIs(tok_Name)) {
        std::string s = tokens.consumeStr();

        if (s == ":nocopy") {
            as_type(result)->nocopy = true;
        } else if (s == ":handle") {
            // :handle is a temporary way to declare a handle type
            setup_handle_type(as_type(result));
            result->setBoolProp("handle", true);
            set_type_property(as_type(result), "handle", &TrueValue);
        } else {
            return compile_error_for_line(result, tokens, startPosition,
                "Unrecognized type attribute: " + s);
        }

        possible_whitespace_or_newline(tokens);
    }

    // if there's a semicolon, then finish it as an empty type.
    if (tokens.nextIs(tok_Semicolon)) {
        result->setBoolProp("syntax:semicolon", true);
        return ParseResult(result);
    }

    if (!tokens.nextIs(tok_LBrace) && !tokens.nextIs(tok_LBracket))
        return compile_error_for_line(result, tokens, startPosition);

    // Parse as compound type
    list_t::setup_type(unbox_type(result));
    int closingToken = tokens.nextIs(tok_LBrace) ? tok_RBrace : tok_RBracket;

    tokens.consume();

    result->setStringProp("syntax:postLBracketWhitespace",
            possible_whitespace_or_newline(tokens));

    Branch* contents = nested_contents(result);

    while (!tokens.nextIs(closingToken)) {
        std::string preWs = possible_whitespace_or_newline(tokens);

        if (tokens.nextIs(closingToken))
            break;

        if (!tokens.nextIs(tok_Identifier))
            return compile_error_for_line(result, tokens, startPosition);

        //std::string fieldTypeName = tokens.consume(tok_Identifier);
        Term* fieldType = type_expr(branch, tokens, context).term;

        std::string postNameWs = possible_whitespace(tokens);

        std::string fieldName;

        if (tokens.nextIs(tok_Identifier))
            fieldName = tokens.consumeStr(tok_Identifier);

        Term* field = create_value(contents, as_type(fieldType), fieldName);

        field->setStringProp("syntax:preWhitespace", preWs);
        field->setStringProp("syntax:postNameWs", postNameWs);
        field->setStringProp("syntax:postWhitespace", possible_statement_ending(tokens));
    }

    tokens.consume(closingToken);

    list_initialize_parameter_from_type_decl(contents, &as_type(result)->parameter);

    return ParseResult(result);
}

ParseResult if_block(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();

    Term* result = apply(branch, FUNCS.if_block, TermList());
    Branch* contents = nested_contents(result);
    if_block_start(contents);

    Term* caseTerm = NULL;
    bool firstIteration = true;
    bool encounteredElse = false;

    while (true) {
        // Consume 'if' or 'elif' or 'else'.
        
        std::string preKeywordWhitespace = possible_whitespace(tokens);

        if (tokens.finished())
            return compile_error_for_line(result, tokens, startPosition);

        int leadingTokenPosition = tokens.getPosition();
        int leadingToken = tokens.next().match;

        // First iteration should always be 'if'
        if (firstIteration)
            ca_assert(leadingToken == tok_If);
        else
            ca_assert(leadingToken != tok_If);

        // Otherwise expect 'elif' or 'else'
        if (leadingToken != tok_If && leadingToken != tok_Elif && leadingToken != tok_Else)
            return compile_error_for_line(result, tokens, startPosition,
                    "Expected 'if' or 'elif' or 'else'");
        tokens.consume();

        bool expectCondition = (leadingToken == tok_If || leadingToken == tok_Elif);

        if (expectCondition) {
            possible_whitespace(tokens);
            Term* condition = infix_expression(branch, tokens, context).term;
            ca_assert(condition != NULL);
            caseTerm = if_block_append_case(contents, condition);
        } else {
            // Create an 'else' block
            encounteredElse = true;
            caseTerm = if_block_append_case(contents, NULL);
            rename(caseTerm, "else");
        }

        caseTerm->setStringProp("syntax:preWhitespace", preKeywordWhitespace);
        set_starting_source_location(caseTerm, leadingTokenPosition, tokens);
        consume_branch(nested_contents(caseTerm), tokens, context);
        branch_finish_changes(nested_contents(caseTerm));

        if (tokens.nextNonWhitespaceIs(tok_Elif)
                || (tokens.nextNonWhitespaceIs(tok_Else) && !encounteredElse)) {

            // If the previous block was multiline, then only parse the next block if
            // it has equal indentation.
            bool wrongIndent = caseTerm->boolPropOptional("syntax:multiline",false)
                && (caseTerm->sourceLoc.col != find_indentation_of_next_statement(tokens));

            if (!wrongIndent) {
                firstIteration = false;
                continue;
            }
        }

        break;
    }

    // If the last block was marked syntax:multiline, then add a lineEnding, so that
    // we don't parse another one.
    if (caseTerm->boolPropOptional("syntax:multiline", false)
            || caseTerm->hasProperty("syntax:lineEnding"))
        result->setStringProp("syntax:lineEnding", "");

    // If we didn't encounter an 'else' block, then create an empty one.
    if (!encounteredElse) {
        Term* elseTerm = if_block_append_case(contents, NULL);
        rename(elseTerm, "else");
        hide_from_source(elseTerm);
    }

    // Move the if_block term to be after the condition terms.
    move_before_final_terms(result);

    finish_if_block(result);
    set_source_location(result, startPosition, tokens);

    return ParseResult(result);
}

ParseResult switch_block(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();

    tokens.consume(tok_Switch);
    possible_whitespace(tokens);

    Term* input = infix_expression(branch, tokens, context).term;

    Term* result = apply(branch, SWITCH_FUNC, TermList(input));

    set_starting_source_location(result, startPosition, tokens);
    consume_branch(nested_contents(result), tokens, context);

    // case_statement may have appended some terms to our branch, so move this
    // term to compensate.
    move_before_final_terms(result);

    switch_block_post_compile(result);
    set_source_location(result, startPosition, tokens);
    set_is_statement(result, true);
    return ParseResult(result);
}

ParseResult case_statement(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();

    tokens.consume(tok_Case);
    possible_whitespace(tokens);

    // Find the parent 'switch' block.
    Term* parent = branch->owningTerm;
    if (parent == NULL || parent->function != SWITCH_FUNC) {
        return compile_error_for_line(branch, tokens, startPosition,
            "'case' keyword must occur inside 'switch' block");
    }

    Branch* parentBranch = parent->owningBranch;

    // Parse the 'case' input, using the branch that the 'switch' is in.
    Term* input = infix_expression(parentBranch, tokens, context).term;

    Term* result = apply(branch, FUNCS.case_func, TermList(input));

    set_starting_source_location(result, startPosition, tokens);
    consume_branch(nested_contents(result), tokens, context);

    set_source_location(result, startPosition, tokens);
    set_is_statement(result, true);
    return ParseResult(result);
}

ParseResult for_block(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();

    tokens.consume(tok_For);
    possible_whitespace(tokens);

    if (!tokens.nextIs(tok_Identifier))
        return compile_error_for_line(branch, tokens, startPosition);

    std::string iterator_name = tokens.consumeStr(tok_Identifier);
    possible_whitespace(tokens);

    if (!tokens.nextIs(tok_In))
        return compile_error_for_line(branch, tokens, startPosition);

    tokens.consume(tok_In);
    possible_whitespace(tokens);

    // check for @ operator
    bool rebindListName = false;
    if (tokens.nextIs(tok_At)) {
        tokens.consume(tok_At);
        rebindListName = true;
        possible_whitespace(tokens);
    }

    Term* listExpr = infix_expression(branch, tokens, context).term;

    Term* forTerm = apply(branch, FUNCS.for_func, TermList(listExpr));
    Branch* contents = nested_contents(forTerm);
    set_starting_source_location(forTerm, startPosition, tokens);
    set_input_syntax_hint(forTerm, 0, "postWhitespace", "");

    forTerm->setBoolProp("modifyList", rebindListName);

    start_building_for_loop(forTerm, iterator_name.c_str());

    consume_branch(contents, tokens, context);

    finish_for_loop(forTerm);
    set_source_location(forTerm, startPosition, tokens);

    // Wrap up the rebound value, if it's a complex lexpr.
    if (rebindListName) {
        Term* lexprRoot = find_lexpr_root(listExpr);
        Term* result = write_setter_chain_from_getter_chain(branch, listExpr, forTerm);
        rename(result, lexprRoot->name);
        result->setBoolProp("syntax:implicitName", true);
    }

    return ParseResult(forTerm);
}

ParseResult while_block(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();

    tokens.consume(tok_While);
    possible_whitespace(tokens);

    Term* conditionExpr = infix_expression(branch, tokens, context).term;

    Term* whileTerm = apply(branch, FUNCS.unbounded_loop, TermList(conditionExpr));
    Branch* contents = nested_contents(whileTerm);
    set_starting_source_location(whileTerm, startPosition, tokens);

    consume_branch(contents, tokens, context);

    finish_while_loop(whileTerm);
    set_source_location(whileTerm, startPosition, tokens);
    return ParseResult(whileTerm);
}

ParseResult do_once_block(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();

    tokens.consume(tok_DoOnce);

    Term* result = apply(branch, DO_ONCE_FUNC, TermList());
    set_starting_source_location(result, startPosition, tokens);
    consume_branch(nested_contents(result), tokens, context);

    return ParseResult(result);
}

ParseResult stateful_value_decl(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();

    tokens.consume(tok_State);
    possible_whitespace(tokens);

    if (!tokens.nextIs(tok_Identifier))
        return compile_error_for_line(branch, tokens, startPosition,
                "Expected identifier after 'state'");

    std::string name = tokens.consumeStr(tok_Identifier);
    possible_whitespace(tokens);

    std::string typeName;

    // check for "state <type> <name>" syntax
    if (tokens.nextIs(tok_Identifier)) {
        typeName = name;
        name = tokens.consumeStr(tok_Identifier);
        possible_whitespace(tokens);
    }

    // Create the declared_state() term.
    Term* result = apply(branch, FUNCS.declared_state, TermList(), name);

    // Lookup the explicit type
    Type* type = &ANY_T;
    if (typeName != "") {
        Term* typeTerm = find_name(branch, typeName.c_str(), -1, NAME_LOOKUP_TYPE);

        if (typeTerm == NULL) {
            result->setStringProp("error:unknownType", typeName);
        } else {
            type = as_type(typeTerm);
        }
    }

    // Possibly consume an expression for the initial value.
    if (tokens.nextIs(tok_Equals)) {
        tokens.consume();
        possible_whitespace(tokens);

        // If the initial value contains any new expressions, then those live inside
        // the term's nested_contents.

        Term* initialValue = infix_expression(nested_contents(result), tokens, context).term;

        if (type != declared_type(initialValue) && type != &ANY_T) {
            initialValue = apply(nested_contents(result), FUNCS.cast, TermList(initialValue));
            initialValue->setBoolProp("hidden", true);
            change_declared_type(initialValue, type);
        }

        append_output_placeholder(nested_contents(result), initialValue);

        // If an initial value was used and no specific type was mentioned, use
        // the initial value's type.
        if (typeName == "" && initialValue->type != &NULL_T) {
            type = initialValue->type;
        }
    }

    check_to_insert_implicit_inputs(result);
    change_declared_type(result, type);
    
    if (typeName != "")
        result->setStringProp("syntax:explicitType", typeName);

    set_source_location(result, startPosition, tokens);
    return ParseResult(result);
}

ParseResult expression_statement(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();

    // Parse an expression
    ParseResult result = name_binding_expression(branch, tokens, context);
    Term* term = result.term;

    // If the result was just the reuse of an existing identifier, create a Copy
    // term so that source is preserved.
    if (result.isIdentifier())
        term = apply(branch, FUNCS.copy, TermList(term));

    // Apply a pending rebind
    if (context->pendingRebind != "") {
        std::string name = context->pendingRebind;
        context->pendingRebind = "";
        rename(term, name);
        term->setStringProp("syntax:rebindOperator", name);
    }

    set_source_location(term, startPosition, tokens);
    set_is_statement(term, true);

    return ParseResult(term);
}

ParseResult include_statement(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();

    tokens.consume(tok_Include);

    possible_whitespace(tokens);

    std::string filename;
    if (tokens.nextIs(tok_String)) {
        filename = tokens.consumeStr(tok_String);
        filename = filename.substr(1, filename.length()-2);
    } else {
        return compile_error_for_line(branch, tokens, startPosition,
                "Expected identifier or string after 'include'");
    }

    Term* filenameTerm = create_string(branch, filename);
    hide_from_source(filenameTerm);

    Term* result = apply(branch, FUNCS.include_func, TermList(filenameTerm));

    return ParseResult(result);
}

ParseResult import_statement(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();

    tokens.consume(tok_Import);

    possible_whitespace(tokens);

    if (!tokens.nextIs(tok_Identifier))
        return compile_error_for_line(branch, tokens, startPosition,
                "Expected string after 'import'");

    Name module = tokens.consumeName(tok_Identifier);

    Term* result = apply(branch, FUNCS.import, TermList());
    result->setStringProp("module", name_to_string(module));

    load_module(name_to_string(module), result);

    return ParseResult(result);
}

ParseResult return_statement(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    tokens.consume(tok_Return);
    std::string postKeywordWs = possible_whitespace(tokens);

    Term* output = NULL;

    bool returnsValue = !is_statement_ending(tokens.next().match) &&
        tokens.next().match != tok_RBrace;

    if (returnsValue)
        output = expression(branch, tokens, context).term;

    branch_add_pack_state(branch);

    Term* result = apply(branch, FUNCS.return_func, TermList(output));

    if (postKeywordWs != " ")
        result->setStringProp("syntax:postKeywordWs", postKeywordWs);
    result->setBoolProp("syntax:returnStatement", true);
    
    return ParseResult(result);
}

ParseResult discard_statement(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();

    tokens.consume(tok_Discard);
    
    Term* enclosingForLoop = find_enclosing_for_loop(branch->owningTerm);

    if (enclosingForLoop == NULL)
        return compile_error_for_line(branch, tokens, startPosition,
                "'discard' can only be used inside a for loop");

    Term* result = apply(branch, FUNCS.discard, TermList());

    set_source_location(result, startPosition, tokens);
    return ParseResult(result);
}
ParseResult break_statement(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();

    tokens.consume(tok_Break);
    
    Term* enclosingForLoop = find_enclosing_for_loop(branch->owningTerm);

    if (enclosingForLoop == NULL)
        return compile_error_for_line(branch, tokens, startPosition,
                "'break' can only be used inside a for loop");

    Term* result = apply(branch, FUNCS.break_func, TermList());

    set_source_location(result, startPosition, tokens);
    return ParseResult(result);
}
ParseResult continue_statement(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();

    tokens.consume(tok_Continue);
    
    Term* enclosingForLoop = find_enclosing_for_loop(branch->owningTerm);

    if (enclosingForLoop == NULL)
        return compile_error_for_line(branch, tokens, startPosition,
                "'continue' can only be used inside a for loop");

    Term* result = apply(branch, FUNCS.continue_func, TermList());

    set_source_location(result, startPosition, tokens);
    return ParseResult(result);
}

ParseResult name_binding_expression(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    // Lookahead for a name binding.
    if (lookahead_match_leading_name_binding(tokens)) {
        int startPosition = tokens.getPosition();

        std::string nameBinding = tokens.consumeStr(tok_Identifier);
        std::string preEqualsSpace = possible_whitespace(tokens);
        tokens.consume(tok_Equals);
        std::string postEqualsSpace = possible_whitespace(tokens);

        ParseResult result = name_binding_expression(branch, tokens, context);
        Term* term = result.term;
        
        // If the term already has a name (this is the case for method syntax
        // and for unknown_identifier), then make a copy.
        if (term->name != "")
            term = apply(branch, FUNCS.copy, TermList(term));

        term->setStringProp("syntax:preEqualsSpace", preEqualsSpace);
        term->setStringProp("syntax:postEqualsSpace", postEqualsSpace);

        rename(term, nameBinding);
        set_source_location(term, startPosition, tokens);
        return ParseResult(term);
    }

    // Otherwise, no name binding.
    return expression(branch, tokens, context);
}

ParseResult expression(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    ParseResult result;

    if (tokens.nextIs(tok_If))
        result = if_block(branch, tokens, context);
    else if (tokens.nextIs(tok_For))
        result = for_block(branch, tokens, context);
    else if (tokens.nextIs(tok_Switch))
        result = switch_block(branch, tokens, context);
    else
        result = infix_expression(branch, tokens, context);

    return result;
}

const int HIGHEST_INFIX_PRECEDENCE = 8;

int get_infix_precedence(int match)
{
    switch(match) {
        case tok_TwoDots:
        case tok_RightArrow:
        case tok_LeftArrow:
            return 8;
        case tok_Star:
        case tok_Slash:
        case tok_DoubleSlash:
        case tok_Percent:
            return 7;
        case tok_Plus:
        case tok_Minus:
            return 6;
        case tok_LThan:
        case tok_LThanEq:
        case tok_GThan:
        case tok_GThanEq:
        case tok_DoubleEquals:
        case tok_NotEquals:
            return 4;
        case tok_And:
        case tok_OR:
            return 3;
        case tok_PlusEquals:
        case tok_MinusEquals:
        case tok_StarEquals:
        case tok_SlashEquals:
            return 2;
        case tok_ColonEquals:
        case tok_Equals:
            return 0;
        default:
            return -1;
    }
}

std::string get_function_for_infix(std::string const& infix)
{
    if (infix == "+") return "add";
    else if (infix == "-") return "sub";
    else if (infix == "*") return "mult";
    else if (infix == "/") return "div";
    else if (infix == "//") return "div_i";
    else if (infix == "%") return "remainder";
    else if (infix == "<") return "less_than";
    else if (infix == "<=") return "less_than_eq";
    else if (infix == ">") return "greater_than";
    else if (infix == ">=") return "greater_than_eq";
    else if (infix == "==") return "equals";
    else if (infix == "or") return "or";
    else if (infix == "and") return "and";
    else if (infix == "=") return "assign";
    else if (infix == ":=") return "unsafe_assign";
    else if (infix == "+=") return "add";
    else if (infix == "-=") return "sub";
    else if (infix == "*=") return "mult";
    else if (infix == "/=") return "div";
    else if (infix == "!=") return "not_equals";
    else if (infix == "<-") return "feedback";
    else if (infix == "..") return "range";
    else return "#unrecognized";
}

ParseResult infix_expression(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    return infix_expression_nested(branch, tokens, context, 0);
}

ParseResult infix_expression_nested(Branch* branch, TokenStream& tokens, ParserCxt* context, int precedence)
{
    int startPosition = tokens.getPosition();

    if (precedence > HIGHEST_INFIX_PRECEDENCE)
        return unary_expression(branch, tokens, context);

    std::string preWhitespace = possible_whitespace(tokens);

    ParseResult leftExpr = infix_expression_nested(branch, tokens, context, precedence+1);

    prepend_whitespace(leftExpr.term, preWhitespace);

    while (!tokens.finished() && get_infix_precedence(tokens.nextNonWhitespace()) == precedence) {

        // Special case: if we have an expression that looks like this:
        // <lexpr><whitespace><hyphen><non-whitespace>
        // Then don't parse it as an infix expression.
        
        if (tokens.nextIs(tok_Whitespace) && tokens.nextIs(tok_Minus, 1) && !tokens.nextIs(tok_Whitespace, 2))
            break;

        std::string preOperatorWhitespace = possible_whitespace(tokens);

        std::string operatorStr = tokens.consumeStr();

        std::string postOperatorWhitespace = possible_whitespace(tokens);

        ParseResult result;

        // Right-apply
        if (operatorStr == "->") {
            if (!tokens.nextIs(tok_Identifier))
                return compile_error_for_line(branch, tokens, startPosition);

            std::string functionName = tokens.consumeStr(tok_Identifier);
            Term* function = find_name(branch, functionName.c_str());

            Term* term = apply(branch, function, TermList(leftExpr.term));

            if (term->function == NULL || term->function->name != functionName)
                term->setStringProp("syntax:functionName", functionName);

            term->setStringProp("syntax:declarationStyle", "arrow-concat");

            set_input_syntax_hint(term, 0, "postWhitespace", preOperatorWhitespace);
            // Can't use preWhitespace of input 1 here, because there is no input 1
            term->setStringProp("syntax:postOperatorWs", postOperatorWhitespace);

            result = ParseResult(term);

        // Left-arrow
        } else if (operatorStr == "<-") {
            ParseResult rightExpr = infix_expression_nested(branch, tokens, context, precedence+1);

            if (!is_function(leftExpr.term))
                throw std::runtime_error("Left side of <- must be a function");

            Stack context;
            evaluate_minimum(&context, leftExpr.term, NULL);

            Term* function = leftExpr.term;

            Term* term = apply(branch, function, TermList(rightExpr.term));

            term->setStringProp("syntax:declarationStyle", "left-arrow");
            term->setStringProp("syntax:preOperatorWs", preOperatorWhitespace);
            set_input_syntax_hint(term, 0, "preWhitespace", postOperatorWhitespace);

            result = ParseResult(term);

        } else {
            ParseResult rightExpr = infix_expression_nested(branch, tokens, context, precedence+1);

            std::string functionName = get_function_for_infix(operatorStr);

            ca_assert(functionName != "#unrecognized");

            bool isRebinding = is_infix_operator_rebinding(operatorStr);

            Term* function = find_name(branch, functionName.c_str());
            Term* term = apply(branch, function, TermList(leftExpr.term, rightExpr.term));
            term->setStringProp("syntax:declarationStyle", "infix");
            term->setStringProp("syntax:functionName", operatorStr);

            set_input_syntax_hint(term, 0, "postWhitespace", preOperatorWhitespace);
            set_input_syntax_hint(term, 1, "preWhitespace", postOperatorWhitespace);

            if (isRebinding) {
                // Just bind the name if left side is an identifier.
                // Example: a += 1
                if (leftExpr.isIdentifier())
                    rename(term, leftExpr.term->name);

                // Set up an assign() term if left side is complex
                // Example: a[0] += 1
                else {
                    Term* assignTerm = apply(branch, FUNCS.assign, TermList(leftExpr.term, term));
                    assignTerm->setStringProp("syntax:rebindOperator", operatorStr);
                    set_is_statement(assignTerm, true);

                    // Move an input's post-whitespace to this term.
                    caValue* existingPostWhitespace =
                        assignTerm->input(1)->inputInfo(0)->properties.get("postWhitespace");

                    if (existingPostWhitespace != NULL)
                        move(existingPostWhitespace,
                            assignTerm->inputInfo(0)->properties.insert("postWhitespace"));

                    Term* lexprRoot = find_lexpr_root(leftExpr.term);
                    rename(assignTerm, lexprRoot->name);
                    
                    term = assignTerm;
                }
            } else if (function == FUNCS.assign) {

                set_is_statement(term, true);
                Term* lexprRoot = find_lexpr_root(leftExpr.term);
                rename(term, lexprRoot->name);
            }

            result = ParseResult(term);
        }

        leftExpr = result;
    }

    if (!leftExpr.isIdentifier())
        set_source_location(leftExpr.term, startPosition, tokens);

    return leftExpr;
}

ParseResult unary_expression(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    // Prefix negation
    if (tokens.nextIs(tok_Minus)) {
        tokens.consume(tok_Minus);
        ParseResult expr = atom_with_subscripts(branch, tokens, context);

        // If the minus sign is on a literal number, then just negate it in place,
        // rather than introduce a neg() operation.
        if (is_value(expr.term) && expr.term->name == "") {
            if (is_int(term_value(expr.term))) {
                set_int(term_value(expr.term), as_int(term_value(expr.term)) * -1);
                return expr;
            }
            else if (is_float(term_value(expr.term))) {
                set_float(term_value(expr.term), as_float(term_value(expr.term)) * -1.0f);
                expr.term->setStringProp("float:original-format",
                    "-" + expr.term->stringProp("float:original-format"));
                return expr;
            }
        }

        Term* result = apply(branch, FUNCS.neg, TermList(expr.term));
        return ParseResult(result);
    }

    // Prefix 'not'
    else if (tokens.nextIs(tok_Not)) {
        tokens.consume(tok_Not);
        std::string postOperatorWs = possible_whitespace(tokens);
        ParseResult expr = atom_with_subscripts(branch, tokens, context);
        Term* result = apply(branch, FUNCS.not_func, TermList(expr.term));
        result->setStringProp("syntax:declarationStyle", "prefix");
        result->setStringProp("syntax:postFunctionWs", postOperatorWs);

        return ParseResult(result);
    }

    return atom_with_subscripts(branch, tokens, context);
}

void function_call_inputs(Branch* branch, TokenStream& tokens, ParserCxt* context,
        TermList& arguments, ListSyntaxHints& inputHints)
{
    // Parse function arguments
    int index = 0;
    while (!tokens.nextIs(tok_RParen) && !tokens.nextIs(tok_RBracket) && !tokens.finished()) {

        inputHints.set(index, "preWhitespace", possible_whitespace_or_newline(tokens));

        if (tokens.nextIs(tok_State)) {
            tokens.consume(tok_State);
            possible_whitespace(tokens);
            
            if (!tokens.nextIs(tok_Equals)) {
                compile_error_for_line(branch, tokens, tokens.getPosition(), "Expected: =");
                return;
            }

            tokens.consume(tok_Equals);
            possible_whitespace(tokens);
            inputHints.set(index, "state", &TrueValue);
            inputHints.set(index, "rebindInput", "t");
        }

        if (lookahead_match_rebind_argument(tokens)) {
            tokens.consume(tok_Ampersand);
            inputHints.set(index, "rebindInput", "t");
        }

        Term* term = expression(branch, tokens, context).term;
        inputHints.set(index, "postWhitespace", possible_whitespace_or_newline(tokens));

        arguments.append(term);

        if (tokens.nextIs(tok_Comma) || tokens.nextIs(tok_Semicolon))
            inputHints.append(index, "postWhitespace", tokens.consumeStr());

        // Might be whitespace after the comma as well
        inputHints.append(index, "postWhitespace", possible_whitespace_or_newline(tokens));

        index++;
    }
}

ParseResult method_call(Branch* branch, TokenStream& tokens, ParserCxt* context, ParseResult root)
{
    int startPosition = tokens.getPosition();

    bool explicitRebindLHS = false;

    if (tokens.nextIs(tok_AtDot)) {
        explicitRebindLHS = true;
        tokens.consume();
    } else if (tokens.nextIs(tok_Dot)) {
        explicitRebindLHS = false;
        tokens.consume();
    } else {
        internal_error("parser::method_call expected '.' or '@.'");
    }
    
    bool rebindLHS = explicitRebindLHS;

    std::string functionName = tokens.consumeStr(tok_Identifier);

    bool hasParens = false;
    if (tokens.nextIs(tok_LParen)) {
        tokens.consume(tok_LParen);
        hasParens = true;
    }

    TermList inputs;
    ListSyntaxHints inputHints;

    // Parse inputs
    if (hasParens) {
        function_call_inputs(branch, tokens, context, inputs, inputHints);
        if (!tokens.nextIs(tok_RParen))
            return compile_error_for_line(branch, tokens, startPosition, "Expected: )");
        tokens.consume(tok_RParen);
    }

    inputs.prepend(root.term);
    inputHints.insert(0);
    Type* rootType = root.term->type;

    // Find the function
    Term* function = find_method(branch, rootType, functionName);

    if (function == NULL) {
        // Method could not be statically found.

        // Temporary behavior, if we can statically resolve to a field access then
        // create a get_field term. This should be changed to use auto-generated
        // methods (one for each field).
        int fieldIndex = list_find_field_index_by_name(rootType, functionName.c_str());
        if (fieldIndex != -1) {
            Term* fieldName = create_string(branch, functionName.c_str());
            Term* term = apply(branch, FUNCS.get_field, TermList(root.term, fieldName));
            set_input_syntax_hint(term, 0, "postWhitespace", "");
            return ParseResult(term);
        }

        // Otherwise create a dynamic_method call.
        function = FUNCS.dynamic_method;
    }

    // If the function is known, then check if the function wants to rebind the name,
    // even if the @. operator was not used. (This is not preferred behavior but we're
    // supporting legacy code while the @. operator is evaluated)
    if (function_input_is_extra_output(as_function(function), 0)) {
        rebindLHS = true;
    }

    // Create the term
    Term* term = apply(branch, function, inputs);

    // If the func is dynamic_method and the rebind operator is used, we'll have to create
    // an extra_output ourselves.
    if (function == FUNCS.dynamic_method && rebindLHS) {
        apply(branch, EXTRA_OUTPUT_FUNC, TermList(term));
    }

    // Possibly rebind the left-hand-side
    if (rebindLHS) {
        // LHS may be a getter-chain
        Term* lexprRoot = find_lexpr_root(term->input(0));
        Term* lhs = write_setter_chain_from_getter_chain(branch, term->input(0),
            get_extra_output(term, 0));
        rename(lhs, lexprRoot->name);
    }

    inputHints.apply(term);
    check_to_insert_implicit_inputs(term);
    term->setStringProp("syntax:functionName", functionName);
    term->setStringProp("syntax:declarationStyle", "method-call");
    if (!hasParens)
        term->setBoolProp("syntax:no-parens", true);
    if (explicitRebindLHS)
        term->setStringProp("syntax:operator", "@.");

    set_source_location(term, startPosition, tokens);
    return ParseResult(term);
}

ParseResult function_call(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();

    ParseResult functionParseResult = identifier_no_create(branch,tokens,context);
    Term* function = functionParseResult.term;
    std::string functionName = functionParseResult.identifierName;

    tokens.consume(tok_LParen);

    TermList inputs;
    ListSyntaxHints inputHints;

    function_call_inputs(branch, tokens, context, inputs, inputHints);

    if (!tokens.nextIs(tok_RParen))
        return compile_error_for_line(branch, tokens, startPosition, "Expected: )");
    tokens.consume(tok_RParen);

    Term* result = apply(branch, function, inputs);

    // Store the function name that they used, if it wasn't the function's
    // actual name (for example, the function might be inside a namespace).
    if (function == NULL || result->function->name != functionName)
        result->setStringProp("syntax:functionName", functionName);

    inputHints.apply(result);
    check_to_insert_implicit_inputs(result);

    return ParseResult(result);
}

ParseResult atom_with_subscripts(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    ParseResult result = atom(branch, tokens, context);

    // Now try to parse a subscript to the atom, this could be:
    //   a[0] 
    //   a.b
    //   a.b()

    bool finished = false;
    while (!finished) {

        int startPosition = tokens.getPosition();

        // Check for a[0], array indexing.
        if (tokens.nextIs(tok_LBracket)) {
            tokens.consume(tok_LBracket);

            std::string postLbracketWs = possible_whitespace(tokens);

            Term* subscript = infix_expression(branch, tokens, context).term;

            if (!tokens.nextIs(tok_RBracket))
                return compile_error_for_line(branch, tokens, startPosition, "Expected: ]");

            tokens.consume(tok_RBracket);

            Term* term = apply(branch, FUNCS.get_index, TermList(result.term, subscript));
            set_input_syntax_hint(term, 0, "postWhitespace", "");
            set_input_syntax_hint(term, 1, "preWhitespace", postLbracketWs);
            term->setBoolProp("syntax:brackets", true);
            set_source_location(term, startPosition, tokens);
            result = ParseResult(term);

        // Check for a.b or a@.b, method call
        } else if (tokens.nextIs(tok_Dot) || tokens.nextIs(tok_AtDot)) {
            result = method_call(branch, tokens, context, result);

        } else {
            // TODO here: function call of an expression

            finished = true;
        }
    }

    return result;
}

bool lookahead_match_whitespace_statement(TokenStream& tokens)
{
    if (tokens.nextIs(tok_Newline)) return true;
    if (tokens.nextIs(tok_Whitespace) && tokens.nextIs(tok_Newline, 1)) return true;
    return false;
}

bool lookahead_match_comment_statement(TokenStream& tokens)
{
    int lookahead = 0;
    if (tokens.nextIs(tok_Whitespace))
        lookahead++;
    return tokens.nextIs(tok_Comment, lookahead);
}

bool lookahead_match_leading_name_binding(TokenStream& tokens)
{
    int lookahead = 0;
    if (!tokens.nextIs(tok_Identifier, lookahead++))
        return false;
    if (tokens.nextIs(tok_Whitespace, lookahead))
        lookahead++;
    if (!tokens.nextIs(tok_Equals, lookahead++))
        return false;
    return true;
}

bool lookahead_match_rebind_argument(TokenStream& tokens)
{
    int lookahead = 0;
    if (!tokens.nextIs(tok_Ampersand, lookahead++))
        return false;
    if (tokens.nextIs(tok_Whitespace, lookahead))
        lookahead++;
    if (!tokens.nextIs(tok_Identifier, lookahead++))
        return false;
    return true;
}

Term* find_lexpr_root(Term* term)
{
    if (term->name != "")
        return term;

    if (term->function == FUNCS.get_index)
        return find_lexpr_root(term->input(0));
    else if (term->function == FUNCS.get_field)
        return find_lexpr_root(term->input(0));
    else if (term->function == FUNCS.dynamic_method)
        return find_lexpr_root(term->input(0));
    else
        return term;
}

ParseResult atom(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();
    ParseResult result;

    // function call?
    if (tokens.nextIs(tok_Identifier) && tokens.nextIs(tok_LParen, 1))
        result = function_call(branch, tokens, context);

    // identifier with rebind?
    else if (tokens.nextIs(tok_At) && tokens.nextIs(tok_Identifier, 1))
        result = identifier_with_rebind(branch, tokens, context);

    // identifier?
    else if (tokens.nextIs(tok_Identifier))
        result = identifier(branch, tokens, context);

    // literal integer?
    else if (tokens.nextIs(tok_Integer))
        result = literal_integer(branch, tokens, context);

    // literal string?
    else if (tokens.nextIs(tok_String))
        result = literal_string(branch, tokens, context);

    // literal bool?
    else if (tokens.nextIs(tok_True) || tokens.nextIs(tok_False))
        result = literal_bool(branch, tokens, context);

    // literal null?
    else if (tokens.nextIs(tok_Null))
        result = literal_null(branch, tokens, context);

    // literal hex?
    else if (tokens.nextIs(tok_HexInteger))
        result = literal_hex(branch, tokens, context);

    // literal float?
    else if (tokens.nextIs(tok_Float))
        result = literal_float(branch, tokens, context);

    // literal color?
    else if (tokens.nextIs(tok_Color))
        result = literal_color(branch, tokens, context);

    // literal list?
    else if (tokens.nextIs(tok_LBracket))
        result = literal_list(branch, tokens, context);

    // literal name?
    else if (tokens.nextIs(tok_Name))
        result = literal_name(branch, tokens, context);

    // plain branch?
    else if (tokens.nextIs(tok_LBrace))
        result = plain_branch(branch, tokens, context);

    // parenthesized expression?
    else if (tokens.nextIs(tok_LParen)) {
        tokens.consume(tok_LParen);
        result = expression(branch, tokens, context);

        if (!tokens.nextIs(tok_RParen))
            return compile_error_for_line(result.term, tokens, startPosition);
        tokens.consume(tok_RParen);
        result.term->setIntProp("syntax:parens", result.term->intProp("syntax:parens") + 1);
    }
    else {
        std::string next;
        if (!tokens.finished())
            next = tokens.consumeStr();
        return compile_error_for_line(branch, tokens, startPosition,
            "Unrecognized expression, (next token = " + next + ")");
    }

    if (!result.isIdentifier())
        set_source_location(result.term, startPosition, tokens);

    return result;
}

ParseResult literal_integer(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();
    std::string text = tokens.consumeStr(tok_Integer);
    int value = strtoul(text.c_str(), NULL, 0);
    Term* term = create_int(branch, value);
    set_source_location(term, startPosition, tokens);
    return ParseResult(term);
}

ParseResult literal_hex(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();
    std::string text = tokens.consumeStr(tok_HexInteger);
    int value = strtoul(text.c_str(), NULL, 0);
    Term* term = create_int(branch, value);
    term->setStringProp("syntax:integerFormat", "hex");
    set_source_location(term, startPosition, tokens);
    return ParseResult(term);
}

ParseResult literal_float(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();
    std::string text = tokens.consumeStr(tok_Float);

    // Parse value with atof
    float value = (float) atof(text.c_str());
    Term* term = create_float(branch, value);

    // Assign a default step value, using the # of decimal figures
    int decimalFigures = get_number_of_decimal_figures(text);
    float step = (float) std::pow(0.1, decimalFigures);
    set_step(term, step);

    // Store the original string
    term->setStringProp("float:original-format", text);

    float mutability = 0.0;

    if (tokens.nextIs(tok_Question)) {
        tokens.consume();
        mutability = 1.0;
    }

    if (mutability != 0.0)
        term->setFloatProp("mutability", mutability);

    set_source_location(term, startPosition, tokens);
    return ParseResult(term);
}

ParseResult literal_string(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();

    std::string text = tokens.consumeStr(tok_String);

    std::string quoteType = text.substr(0,1);

    Value escaped;
    unquote_and_unescape_string(text.c_str(), &escaped);

    Term* term = create_string(branch, as_cstring(&escaped));
    set_source_location(term, startPosition, tokens);

    if (quoteType != "'")
        term->setStringProp("syntax:quoteType", quoteType);
    if (!string_eq(&escaped, text.c_str()))
        term->setStringProp("syntax:originalString", text);

    return ParseResult(term);
}

ParseResult literal_bool(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();
    bool value = tokens.nextIs(tok_True);

    tokens.consume();

    Term* term = create_bool(branch, value);
    set_source_location(term, startPosition, tokens);
    return ParseResult(term);
}

ParseResult literal_null(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();

    tokens.consume(tok_Null);

    Term* term = create_value(branch, &NULL_T);
    set_source_location(term, startPosition, tokens);
    return ParseResult(term);
}

int hex_digit_to_number(char digit)
{
    if (digit >= '0' && digit <= '9')
        return digit - '0';

    digit = tolower(digit);

    if (digit >= 'a' && digit <= 'f')
        return 10 + (digit - 'a');

    return 0;
}

int two_hex_digits_to_number(char digit1, char digit2)
{
    return hex_digit_to_number(digit1) * 16 + hex_digit_to_number(digit2);
}

ParseResult literal_color(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();

    std::string text = tokens.consumeStr(tok_Color);

    // strip leading # sign
    text = text.substr(1, text.length()-1);

    Term* resultTerm = create_value(branch, TYPES.color);
    List* result = List::checkCast(term_value(resultTerm));

    float r = 0;
    float g = 0;
    float b = 0;
    float a = 0;

    if (text.length() == 3 || text.length() == 4) {
        r = hex_digit_to_number(text[0]) / 15.0f;
        g = hex_digit_to_number(text[1]) / 15.0f;
        b = hex_digit_to_number(text[2]) / 15.0f;

        // optional alpha
        if (text.length() == 3)
            a = 1.0;
        else
            a = hex_digit_to_number(text[3]) / 15.0f;
    } else {
        r = two_hex_digits_to_number(text[0], text[1]) / 255.0f;
        g = two_hex_digits_to_number(text[2], text[3]) / 255.0f;
        b = two_hex_digits_to_number(text[4], text[5]) / 255.0f;

        // optional alpha
        if (text.length() == 6)
            a = 1.0;
        else
            a = two_hex_digits_to_number(text[6], text[7]) / 255.0f;
    }

    set_float(list_get(result, 0), r);
    set_float(list_get(result, 1), g);
    set_float(list_get(result, 2), b);
    set_float(list_get(result, 3), a);

    resultTerm->setIntProp("syntax:colorFormat", (int) text.length());

    set_source_location(resultTerm, startPosition, tokens);
    return ParseResult(resultTerm);
}

ParseResult literal_list(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();

    tokens.consume(tok_LBracket);

    TermList inputs;
    ListSyntaxHints listHints;

    function_call_inputs(branch, tokens, context, inputs, listHints);

    if (!tokens.nextIs(tok_RBracket))
        return compile_error_for_line(branch, tokens, startPosition, "Expected: ]");
    tokens.consume(tok_RBracket);

    Term* term = apply(branch, FUNCS.list, inputs);
    listHints.apply(term);
    check_to_insert_implicit_inputs(term);

    term->setBoolProp("syntax:literal-list", true);
    term->setStringProp("syntax:declarationStyle", "bracket-list");
    set_source_location(term, startPosition, tokens);

    return ParseResult(term);
}

ParseResult literal_name(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();

    std::string s = tokens.nextStr();
    tokens.consume(tok_Name);

    Term* term = create_value(branch, &NAME_T);

    // Skip the leading ':' in the name string
    set_name(term_value(term), name_from_string(s.c_str() + 1));
    set_source_location(term, startPosition, tokens);

    return ParseResult(term);
}

ParseResult plain_branch(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();
    Term* term = apply(branch, FUNCS.lambda, TermList());
    Branch* resultBranch = nested_contents(term);
    set_source_location(term, startPosition, tokens);
    consume_branch_with_braces(resultBranch, tokens, context, term);
    create_inputs_for_outer_references(term);
    branch_finish_changes(resultBranch);
    return ParseResult(term);
}

ParseResult namespace_block(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();
    tokens.consume(tok_Namespace);
    possible_whitespace(tokens);

    if (!tokens.nextIs(tok_Identifier))
        return compile_error_for_line(branch, tokens, startPosition,
            "Expected identifier after 'namespace'");

    std::string name = tokens.consumeStr(tok_Identifier);
    Term* term = apply(branch, FUNCS.namespace_func, TermList(), name);
    set_starting_source_location(term, startPosition, tokens);

    consume_branch(nested_contents(term), tokens, context);

    branch_finish_changes(nested_contents(term));

    return ParseResult(term);
}

ParseResult unknown_identifier(Branch* branch, std::string const& name)
{
    Term* term = apply(branch, UNKNOWN_IDENTIFIER_FUNC, TermList(), name);
    set_is_statement(term, false);
    term->setStringProp("message", name);
    return ParseResult(term);
}

ParseResult identifier(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    int startPosition = tokens.getPosition();
    
    std::string id = tokens.consumeStr(tok_Identifier);

    Term* term = find_name(branch, id.c_str());
    if (term == NULL) {
        ParseResult result = unknown_identifier(branch, id);
        set_source_location(result.term, startPosition, tokens);
        return result;
    }

    return ParseResult(term, id);
}

ParseResult identifier_with_rebind(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    //int startPosition = tokens.getPosition();

    bool rebindOperator = false;

    if (tokens.nextIs(tok_At)) {
        tokens.consume(tok_At);
        rebindOperator = true;
    }

    std::string id = tokens.consumeStr(tok_Identifier);

    Term* head = find_name(branch, id.c_str());
    ParseResult result;

    if (head == NULL)
        result = unknown_identifier(branch, id);
    else
        result = ParseResult(head, id);

    if (rebindOperator)
        context->pendingRebind = result.term->name;

    return result;
}

// Consume an IDENTIFIER, but if the name is not found, don't create an
// unknown_identifier() call. Instead just return a ParseResult with
// a NULL term.
ParseResult identifier_no_create(Branch* branch, TokenStream& tokens, ParserCxt* context)
{
    std::string id = tokens.consumeStr(tok_Identifier);
    Term* term = find_name(branch, id.c_str());
    // term may be NULL
    return ParseResult(term, id);
}

// --- More Utility functions ---

void prepend_whitespace(Term* term, std::string const& whitespace)
{
    if (whitespace != "" && term != NULL) {
        std::string s = whitespace + term->stringProp("syntax:preWhitespace");
        term->setStringProp("syntax:preWhitespace", s.c_str());
    }
}

void append_whitespace(Term* term, std::string const& whitespace)
{
    if (whitespace != "" && term != NULL) {
        std::string s = term->stringProp("syntax:postWhitespace") + whitespace;
        term->setStringProp("syntax:postWhitespace", s.c_str());
    }
}

void set_starting_source_location(Term* term, int start, TokenStream& tokens)
{
    term->sourceLoc.col = tokens[start].colStart;
    term->sourceLoc.line = tokens[start].lineStart;
}

void set_source_location(Term* term, int start, TokenStream& tokens)
{
    ca_assert(term != NULL);
    ca_assert(tokens.length() != 0);

    TermSourceLocation loc;

    if (start >= tokens.length()) {
        // 'start' is at the end of the stream
        loc.col = tokens[start-1].colEnd+1;
        loc.line = tokens[start-1].lineEnd;

    } else {
        loc.col = tokens[start].colStart;
        loc.line = tokens[start].lineStart;
    }

    int end = tokens.getPosition();
    if (end >= tokens.length()) end = tokens.length()-1;

    loc.colEnd = tokens[end].colEnd;
    loc.lineEnd = tokens[end].lineEnd;

    term->sourceLoc.grow(loc);
}

std::string consume_line(TokenStream &tokens, int start, Term* positionRecepient)
{
    ca_assert(start <= tokens.getPosition());

    int originalPosition = tokens.getPosition();

    tokens.resetPosition(start);

    std::stringstream line;
    while (!tokens.finished()) {

        // If we've passed our originalPosition and reached a newline, then stop
        if (tokens.getPosition() > originalPosition
                && (tokens.nextIs(tok_Newline) || tokens.nextIs(tok_Semicolon)))
            break;

        line << tokens.consumeStr();
    }

    // throw out trailing newline
    if (!tokens.finished())
        tokens.consume();

    // make sure we passed our original position
    ca_assert(tokens.getPosition() >= originalPosition);

    if (positionRecepient != NULL)
        set_source_location(positionRecepient, start, tokens);

    return line.str();
}

Term* insert_compile_error(Branch* branch, TokenStream& tokens,
        std::string const& message)
{
    Term* result = apply(branch, UNRECOGNIZED_EXPRESSION_FUNC, TermList());
    result->setStringProp("message", message);
    set_source_location(result, tokens.getPosition(), tokens);
    return result;
}

ParseResult compile_error_for_line(Branch* branch, TokenStream& tokens, int start,
        std::string const& message)
{
    Term* result = apply(branch, UNRECOGNIZED_EXPRESSION_FUNC, TermList());
    return compile_error_for_line(result, tokens, start, message);
}

ParseResult compile_error_for_line(Term* existing, TokenStream &tokens, int start,
        std::string const& message)
{
    if (existing->function != UNRECOGNIZED_EXPRESSION_FUNC)
        change_function(existing, UNRECOGNIZED_EXPRESSION_FUNC);
    std::string line = consume_line(tokens, start, existing);

    existing->setStringProp("originalText", line.c_str());
    existing->setStringProp("message", message.c_str());

    ca_assert(has_static_error(existing));

    return ParseResult(existing);
}

bool is_infix_operator_rebinding(std::string const& infix)
{
    return (infix == "+=" || infix == "-=" || infix == "*=" || infix == "/=");
}

std::string possible_whitespace(TokenStream& tokens)
{
    if (tokens.nextIs(tok_Whitespace))
        return tokens.consumeStr(tok_Whitespace);
    else
        return "";
}

std::string possible_newline(TokenStream& tokens)
{
    if (tokens.nextIs(tok_Newline))
        return tokens.consumeStr(tok_Newline);
    else
        return "";
}

std::string possible_whitespace_or_newline(TokenStream& tokens)
{
    std::stringstream output;

    while (tokens.nextIs(tok_Newline) || tokens.nextIs(tok_Whitespace))
        output << tokens.consumeStr();

    return output.str();
}

bool is_statement_ending(int t)
{
    return t == tok_Comma || t == tok_Semicolon || t == tok_Newline;
}

std::string possible_statement_ending(TokenStream& tokens)
{
    std::stringstream result;
    if (tokens.nextIs(tok_Whitespace))
        result << tokens.consumeStr();

    if (tokens.nextIs(tok_Comma) || tokens.nextIs(tok_Semicolon))
        result << tokens.consumeStr();

    if (tokens.nextIs(tok_Whitespace))
        result << tokens.consumeStr();

    if (tokens.nextIs(tok_Newline))
        result << tokens.consumeStr(tok_Newline);

    return result.str();
}

bool is_multiline_block(Term* term)
{
    return term->boolPropOptional("syntax:multiline", false);
}

int get_number_of_decimal_figures(std::string const& str)
{
    bool dotFound = false;
    int result = 0;

    for (int i=0; str[i] != 0; i++) {
        if (str[i] == '.') {
            dotFound = true;
            continue;
        }

        if (dotFound)
            result++;
    }

    if (result == 0 && dotFound)
        result = 1;

    return result;
}

void unquote_and_unescape_string(const char* input, caValue* out)
{
    if (input[0] == 0)
        return;

    char quote = input[0];

    int quoteSize = 1;
    if (quote == '<')
        quoteSize = 3;

    int end = strlen(input) - quoteSize;

    // Unescape any escaped characters
    std::stringstream result;
    for (int i=quoteSize; i < end; i++) {
        char c = input[i];
        char next = 0;
        if (i + 1 < end)
            next = input[i+1];

        if (c == '\\') {
            if (next == 'n') {
                result << '\n';
                i++;
            } else if (next == '\'') {
                result << '\'';
                i++;
            } else if (next == '\"') {
                result << '\"';
                i++;
            } else if (next == '\\') {
                result << '\\';
                i++;
            } else {
                result << c;
            }
        } else {
            result << c;
        }
    }

    set_string(out, result.str());
}

void quote_and_escape_string(const char* input, caValue* out)
{
    std::stringstream result;

    result << '"';

    for (int i=0; input[i] != 0; i++) {
        if (input[i] == '\n')
            result << "\\n";
        else if (input[i] == '\'')
            result << "\\'";
        else if (input[i] == '"')
            result << "\\\"";
        else if (input[i] == '\\')
            result << "\\\\";
        else
            result << input[i];
    }

    result << '"';

    set_string(out, result.str());
}

} // namespace parser

} // namespace circa
