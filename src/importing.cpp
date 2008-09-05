
#include "ast.h"
#include "branch.h"
#include "builtins.h"
#include "importing.h"
#include "operations.h"
#include "parser.h"
#include "term.h"
#include "token_stream.h"

namespace circa {

Term* import_c_function(Branch* branch, Function::ExecuteFunc execute, std::string const& headerText)
{
    token_stream::TokenStream tokens(headerText);
    ast::FunctionHeader *header = parser::functionHeader(tokens);

    Term* term = create_constant(branch, FUNCTION_TYPE);
    Function* func = as_function(term);

    func->name = header->functionName;
    func->execute = execute;

    TermList inputTypes;

    ast::FunctionHeader::ArgumentList::iterator it;
    for (it = header->arguments.begin(); it != header->arguments.end(); ++it) {
        std::string typeName = it->type;
        Term* type = branch->getNamed(typeName);
        as_type(type);
        inputTypes.append(type);
    }

    Term* outputType = NULL;
    if (header->outputType != "")
        outputType = branch->getNamed(header->outputType);
    else
        outputType = VOID_TYPE;

    func->inputTypes = inputTypes;
    func->outputType = outputType;
    branch->bindName(term, header->functionName);
    return term;
}

}
