// Copyright 2008 Andrew Fischer

#include "common_headers.h"

#include "ast.h"
#include "branch.h"
#include "builtins.h"
#include "compilation.h"
#include "cpp_importing.h"
#include "function.h"
#include "parser.h"
#include "list.h"
#include "runtime.h"
#include "set.h"
#include "term.h"
#include "token_stream.h"
#include "type.h"
#include "wrappers.h"
#include "values.h"

namespace circa {
namespace ast {

Infix::~Infix()
{
    delete left;
    delete right;
}

Term*
Infix::createTerm(Branch &branch)
{
    // special case for dot operator.
    if (this->operatorStr == ".") {
        return create_dot_concatenated_call(branch, *this);
    }

    // special case for ->
    if (this->operatorStr == "->") {
        return create_arrow_concatenated_call(branch, *this);
    }

    // special case for :=
    if (this->operatorStr == ":=") {
        return create_feedback_call(branch, *this);
    }

    return create_infix_call(branch, *this);
}

FunctionCall::~FunctionCall()
{
    ArgumentList::iterator it;
    for (it = this->arguments.begin(); it != this->arguments.end(); ++it) {
        delete (*it);
    }
}

void
FunctionCall::addArgument(Expression* expr, std::string const& preWhitespace,
            std::string const& postWhitespace)
{
    Argument *arg = new Argument();
    arg->expression = expr;
    arg->preWhitespace = preWhitespace;
    arg->postWhitespace = postWhitespace;
    this->arguments.push_back(arg);
}

Term*
FunctionCall::createTerm(Branch &branch)
{
    return create_function_call(branch, *this);
}

Term*
LiteralString::createTerm(Branch &branch)
{
    return create_literal_string(branch, *this);
}

Term*
LiteralFloat::createTerm(Branch &branch)
{
    return create_literal_float(branch, *this);
}

Term*
LiteralInteger::createTerm(Branch &branch)
{
    return create_literal_integer(branch, *this);
}

Term*
Identifier::createTerm(Branch &branch)
{
    Term* result = find_named(&branch,this->text);

    if (result == NULL) {
        // parser::syntax_error(std::string("Couldn't find identifier: ") + this->text);
        return NULL;
    }

    if (hasRebindOperator) {
        push_pending_rebind(branch, this->text);
    }

    return result;
}

Term*
ExpressionStatement::createTerm(Branch &branch)
{
    // Special case: when we are just renaming an identifier.
    // Create a call to "copy"
    if (expression->typeName() == "Identifier" && nameBinding != "") {
        Term* identifiedTerm = expression->createTerm(branch);
        Term* result = apply_function(&branch, COPY_FUNC, ReferenceList(identifiedTerm));
        branch.bindName(result, nameBinding);
        return result;
    }

    bool prev = push_is_inside_expression(branch, false);
    Term* term = this->expression->createTerm(branch);
    pop_is_inside_expression(branch, prev);
    
    if (this->nameBinding != "")
        branch.bindName(term, this->nameBinding);

    std::string pendingRebind = get_pending_rebind(branch);
    if (pendingRebind != "") {
        branch.bindName(term, pendingRebind);
        branch.removeTerm(get_name_for_attribute("comp-pending-rebind"));
    }

    term->syntaxHints.precedingWhitespace = this->precedingWhitespace;

    return term;
}

Term*
CommentStatement::createTerm(Branch &branch)
{
    return create_comment(branch, this->text);
}

StatementList::~StatementList()
{
    Statement::Vector::iterator it;
    for (it = statements.begin(); it != statements.end(); ++it) {
        delete (*it);
    }
}

void
StatementList::push(Statement* statement)
{
    this->statements.push_back(statement);
}

void
StatementList::createTerms(Branch &branch)
{
    Statement::Vector::const_iterator it;
    for (it = statements.begin(); it != statements.end(); ++it) {
        (*it)->createTerm(branch);
    }
}

void
FunctionHeader::addArgument(std::string const& type, std::string const& name)
{
    Argument arg;
    arg.type = type;
    arg.name = name;
    this->arguments.push_back(arg);
}

FunctionDecl::~FunctionDecl()
{
    delete this->header;
    delete this->statements;
}

Term*
FunctionDecl::createTerm(Branch &branch)
{
    Term* resultTerm = create_value(&branch, FUNCTION_TYPE, this->header->functionName);
    Function& result = as_function(resultTerm);
    result.name = header->functionName;
    result.evaluate = Function::subroutine_call_evaluate;
    result.stateType = BRANCH_TYPE;

    for (unsigned int inputIndex=0;
         inputIndex < this->header->arguments.size();
         inputIndex++)
    {
        FunctionHeader::Argument &arg = this->header->arguments[inputIndex];
        Term* term = find_named(&branch, arg.type);
        if (term == NULL)
            parser::syntax_error(std::string("Identifier not found (input type): ") + arg.type);

        if (!is_type(term))
            parser::syntax_error(std::string("Identifier is not a type: ") + arg.type);

        result.inputTypes.append(term);

        Function::InputProperties inputProps;

        std::string inputName = arg.name;
        if (inputName == "")
            inputName = get_placeholder_name_for_index(inputIndex);

        inputProps.name = inputName;
        result.inputProperties.push_back(inputProps);
    }

    if (this->header->outputType == "") {
        result.outputType = VOID_TYPE;
    } else {
        result.outputType = find_named(&branch,this->header->outputType);
        if (result.outputType == NULL)
            parser::syntax_error(std::string("Identifier not found (output type): ") + this->header->outputType);
        if (!is_type(result.outputType))
            parser::syntax_error(std::string("Identifier is not a type: ") + this->header->outputType);
    }

    // Syntax hints
    resultTerm->syntaxHints.declarationStyle = TermSyntaxHints::LITERAL_VALUE;

    // Create input placeholders
    for (unsigned int inputIndex=0;
         inputIndex < this->header->arguments.size();
         inputIndex++)
    {
        std::string name = result.inputProperties[inputIndex].name;

        create_value(&result.subroutineBranch, result.inputTypes[inputIndex], name);
    }

    result.subroutineBranch.outerScope = &branch;

    // Apply every statement
    int numStatements = this->statements->count();
    for (int statementIndex=0; statementIndex < numStatements; statementIndex++) {
        Statement* statement = this->statements->operator[](statementIndex);

        statement->createTerm(result.subroutineBranch);
    }

    remove_compilation_attrs(result.subroutineBranch);

    return resultTerm;
}

Term*
TypeDecl::createTerm(Branch &branch)
{
    Term* result_term = create_value(&branch, TYPE_TYPE);
    Type &type = as_type(result_term);
    type.makeCompoundType(this->name);

    MemberList::const_iterator it;
    for (it = members.begin(); it != members.end(); ++it) {
        type.addField(find_named(&branch,it->type), it->name);
        type.syntaxHints.addField(it->name);
    }

    branch.bindName(result_term, this->name);
    return result_term;
}

IfStatement::~IfStatement()
{
    delete condition;
    delete positiveBranch;
    delete negativeBranch;
}

Term*
IfStatement::createTerm(Branch &branch)
{
    assert(this->condition != NULL);
    assert(this->positiveBranch != NULL);

    bool prev = push_is_inside_expression(branch, true);
    Term* conditionTerm = this->condition->createTerm(branch);
    pop_is_inside_expression(branch, prev);

    Term* ifStatementTerm = apply_function(&branch,
                                           "if-statement",
                                           ReferenceList(conditionTerm));

    Branch& posBranch = as_branch(ifStatementTerm->state->field(0));
    Branch& negBranch = as_branch(ifStatementTerm->state->field(1));
    Branch& joiningTermsBranch = as_branch(ifStatementTerm->state->field(2));

    posBranch.outerScope = &branch;
    negBranch.outerScope = &branch;
    joiningTermsBranch.outerScope = &branch;

    this->positiveBranch->createTerms(posBranch);
    remove_compilation_attrs(posBranch);

    if (this->negativeBranch != NULL) {
        this->negativeBranch->createTerms(negBranch);
        remove_compilation_attrs(negBranch);
    }

    // Create joining terms

    // First, get a list of all names that were bound in these branches

    Branch workspace;

    import_value(workspace, BRANCH_TYPE, &posBranch, "posBranch");
    import_value(workspace, BRANCH_TYPE, &negBranch, "negBranch");

    workspace.eval("names = posBranch.get-branch-bound-names");
    workspace.eval("list-join(@names, negBranch.get-branch-bound-names)");

    workspace.eval("list-remove-duplicates(@names)");

    List& names = as<List>(workspace["names"]);
    Branch& outerBranch = branch;

    // Remove any names which are not bound in the outer branch
    for (int i=0; i < names.count(); i++) {
        if (!outerBranch.containsName(as_string(names[i]))) {
            names.remove(i--);
        }
    }

    // For each name, create joining term
    for (int i=0; i < names.count(); i++) {
        std::string &name = as_string(names[i]);

        Term* posTerm = posBranch.containsName(name) ?
            posBranch[name] : find_named(&outerBranch, name);
        Term* negTerm = negBranch.containsName(name) ?
            negBranch[name] : find_named(&outerBranch, name);

        Term* joining_term = apply_function(&joiningTermsBranch,
                "if-expr",
                ReferenceList(conditionTerm, posTerm, negTerm));
        outerBranch.bindName(joining_term, name);
    }

    return ifStatementTerm;
}

Term* StatefulValueDeclaration::createTerm(Branch &branch)
{
    return create_stateful_value_declaration(branch, *this);
}

void
output_ast_to_string(ASTNode *node, std::stringstream &out)
{
    int numChildren = node->numChildren();

    if (numChildren > 0) {
        out << "(" << node->typeName();
        for (int i=0; i < numChildren; i++)
        {
            out << " ";
            output_ast_to_string(node->getChild(i), out);
        }
        out << ")";
    } else {
        out << node->typeName();
    }
}

std::string
print_ast(ASTNode *node)
{
    std::stringstream stream;
    output_ast_to_string(node, stream);
    return stream.str();
}

} // namespace ast
} // namespace circa
