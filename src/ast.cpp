// Copyright 2008 Andrew Fischer

#include "common_headers.h"

#include "ast.h"
#include "branch.h"
#include "builtins.h"
#include "parser.h"
#include "runtime.h"
#include "term.h"
#include "token_stream.h"
#include "type.h"
#include "values.h"

namespace circa {
namespace ast {

Term* find_and_apply_function(Branch& branch, std::string const& functionName,
        ReferenceList inputs)
{
    Term* function = branch.findNamed(functionName);

    if (function == NULL) {
        Term* result = apply_function(branch, UNKNOWN_FUNCTION, inputs);
        as_string(result->state) = functionName;
        return result;
    }

    return apply_function(branch, function, inputs);
}

Infix::~Infix()
{
    delete left;
    delete right;
}

std::string
Infix::toString() const
{
    if (left == NULL) return "<error, left is NULL>";
    if (right == NULL) return "<error, right is NULL>";
    return this->left->toString() + this->preOperatorWhitespace
        + this->operatorStr
        + this->postOperatorWhitespace
        + this->right->toString();
}
Term*
Infix::createTerm(Branch& branch)
{
    // special case: right arrow
    if (this->operatorStr == "->") {
        Term* leftTerm = this->left->createTerm(branch);

        Identifier *rightIdent = dynamic_cast<Identifier*>(this->right);

        if (rightIdent == NULL) {
            parser::syntax_error("Right side of -> must be an identifier");
        }

        return find_and_apply_function(branch, rightIdent->text, ReferenceList(leftTerm));
    }

    // todo
    parser::syntax_error("Infix is unimplemented");

    return NULL; // unreachable
}

void
Infix::walk(ExpressionWalker& walker)
{
    walker.visit(this);
    left->walk(walker);
    right->walk(walker);
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

std::string
FunctionCall::toString() const
{
    std::stringstream output;
    output << this->functionName << "(";

    bool firstArg = true;
    ArgumentList::const_iterator it;
    for (it = arguments.begin(); it != arguments.end(); ++it) {
        if (!firstArg)
            output << ",";
        output << (*it)->preWhitespace << (*it)->expression->toString()
            << (*it)->postWhitespace;
        firstArg = false;
    }
    output << ")";
    return output.str();
}

Term*
FunctionCall::createTerm(Branch& branch)
{
    ReferenceList inputs;

    for (unsigned int i=0; i < arguments.size(); i++) {
        Argument* arg = arguments[i];
        Term* term = arg->expression->createTerm(branch);
        assert(term != NULL);
        inputs.append(term);
    }

    return find_and_apply_function(branch, this->functionName, inputs);
}

void
FunctionCall::walk(ExpressionWalker& walker)
{
    walker.visit(this);
    ArgumentList::iterator it;
    for (it = arguments.begin(); it != arguments.end(); ++it) {
        if (*it == NULL)
            throw std::runtime_error("argument is null");
        Expression* expr = (*it)->expression;
        if (expr == NULL)
            throw std::runtime_error("expression is null");
        expr->walk(walker);
    }
}

std::string
LiteralString::toString() const
{
    return "\"" + this->text + "\"";
}

Term*
LiteralString::createTerm(Branch& branch)
{
    return string_var(branch, this->text);
}

Term*
LiteralFloat::createTerm(Branch& branch)
{
    float value = atof(this->text.c_str());
    Term* term = float_var(branch, value);
    float mutability = hasQuestionMark ? 1.0 : 0.0;
    term->properties.addSlot("mutability", FLOAT_TYPE)->asFloat() = mutability;
    return term;
}

Term*
LiteralInteger::createTerm(Branch& branch)
{
    int value = atoi(this->text.c_str());
    return int_var(branch, value);
}

std::string
Identifier::toString() const
{
    std::string rebindSymbol = "";
    if (hasRebindOperator)
        rebindSymbol = "@";
    return rebindSymbol + text;
}
Term*
Identifier::createTerm(Branch& branch)
{
    return branch.findNamed(this->text);
}

std::string
ExpressionStatement::toString() const
{
    std::string output;

    if (nameBinding != "") {
        output = nameBinding + preEqualsWhitepace + "=" + postEqualsWhitespace;
    }

    output += expression->toString();
    
    return output;
}

Term*
ExpressionStatement::createTerm(Branch& branch)
{
    Term* term = this->expression->createTerm(branch);
    
    if (this->nameBinding != "")
        branch.bindName(term, this->nameBinding);

    return term;
}

StatementList::~StatementList()
{
    Statement::List::iterator it;
    for (it = statements.begin(); it != statements.end(); ++it) {
        delete (*it);
    }
}

void
StatementList::push(Statement* statement)
{
    this->statements.push_back(statement);
}

std::string
StatementList::toString() const
{
    std::stringstream output;

    Statement::List::const_iterator it;
    for (it = statements.begin(); it != statements.end(); ++it) {
        output << (*it)->toString() << "\n";
    }
    return output.str();
}

void
StatementList::createTerms(Branch& branch)
{
    Statement::List::const_iterator it;
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

std::string
FunctionHeader::toString() const
{
    std::stringstream out;
    out << "function " << functionName << "(";
    ArgumentList::const_iterator it;
    bool first = true;
    for (it = arguments.begin(); it != arguments.end(); ++it) {
        if (!first) out << ", ";
        out << it->type << " " << it->name;
        first = false;
    }
    out << ")";
    return out.str();
}

FunctionDecl::~FunctionDecl()
{
    delete this->header;
    delete this->statements;
}

Term*
FunctionDecl::createTerm(Branch& branch)
{
    Branch workspace;

    eval_statement(workspace, "inputTypes = list()");

    FunctionHeader::ArgumentList::const_iterator it;
    for (it = this->header->arguments.begin(); it != this->header->arguments.end(); ++it)
    {
        Term* term = branch.findNamed(it->type);
        if (term == NULL)
            parser::syntax_error(std::string("Identifier not found (input type): ") + it->type);

        if (!is_type(term))
            parser::syntax_error(std::string("Identifier is not a type: ") + it->type);

        workspace.bindName(term, "t");
        eval_statement(workspace, "list-append(@inputTypes, t)");
    }

    Term* outputType = NULL;

    if (this->header->outputType == "") {
        outputType = VOID_TYPE;
    } else {
        outputType = branch.findNamed(this->header->outputType);
        if (outputType == NULL)
            parser::syntax_error(std::string("Identifier not found (output type): ") + this->header->outputType);
        if (!is_type(outputType))
            parser::syntax_error(std::string("Identifier is not a type: ") + this->header->outputType);
    }

    // Load into workspace
    string_var(workspace, this->header->functionName, "functionName");
    workspace.bindName(outputType, "outputType");

    // Create
    Term* sub = eval_statement(workspace,
            "sub = subroutine-create(functionName, inputTypes, outputType)");

    // Name inputs
    

    int numStatements = this->statements->count();
    for (int statementIndex=0; statementIndex < numStatements; statementIndex++) {
        //Statement* statement = this->statements->operator[](statementIndex);

        // FIXME statement->createTerm(as_subroutine(sub)->branch);

        //string_var(&workspace, statement->text, "statement");
        //eval_statement(workspace,
                //"subroutine-eval(@sub, statement)");
    }

    branch.bindName(sub, this->header->functionName);

    return sub;
}

std::string
FunctionDecl::toString() const
{
    std::stringstream out;

    out << header->toString() << std::endl << "{" << std::endl;

    out << statements->toString();

    out << "}";

    return out.str();
}

void initialize_ast_functions(Branch& kernel)
{

}

} // namespace ast
} // namespace circa
