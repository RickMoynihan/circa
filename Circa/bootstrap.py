
import string, os, pdb

from Circa import (
    builtins,
    code,
    ca_type,
    ca_function,
    parser,
    python_bridge,
    signature,
    token
)

# todo
CIRCA_HOME = "/Users/andyfischer/code/circa"

VERBOSE_DEBUGGING = False

builtins.BUILTINS = code.CodeUnit()

# Create 'constant' function, which temporarily does not have a function
builtins.CONST_FUNC = code.Term()
builtins.CONST_FUNC.codeUnit = builtins.BUILTINS
builtins.CONST_FUNC.functionTerm = builtins.CONST_FUNC
builtins.BUILTINS.setTermName(builtins.CONST_FUNC, "constant")
builtins.CONST_FUNC.globalID = 1
builtins.CONST_FUNC.pythonValue = ca_function.createFunction(inputs=[], output=None)

# Create 'constant-Type' function
builtins.CONST_TYPE_FUNC = builtins.BUILTINS.createTerm(functionTerm=builtins.CONST_FUNC)
builtins.CONST_TYPE_FUNC.pythonValue = ca_function.createFunction(inputs=[], output=None)

# Create Type type
builtins.TYPE_TYPE = builtins.BUILTINS.createTerm(functionTerm=builtins.CONST_TYPE_FUNC,
    name = 'Type', initialValue = ca_type.Type())
builtins.TYPE_TYPE.pythonValue.outputType=builtins.TYPE_TYPE

# Implant the Type type
builtins.CONST_FUNC.pythonValue.inputs=[builtins.TYPE_TYPE]
builtins.CONST_TYPE_FUNC.pythonValue.outputType=builtins.TYPE_TYPE

# Create Function type
builtins.FUNC_TYPE = builtins.BUILTINS.createConstant(name = 'Function',
    value = ca_type.Type(),
    type=builtins.TYPE_TYPE)

# Implant types into 'constant' function
builtins.CONST_FUNC.pythonValue.inputTypes = [builtins.TYPE_TYPE]
builtins.CONST_FUNC.pythonValue.outputType = builtins.FUNC_TYPE

# Create basic types
builtins.INT_TYPE = builtins.BUILTINS.createConstant(name = 'int',
    value = ca_type.Type(),
    type=builtins.TYPE_TYPE)
builtins.FLOAT_TYPE = builtins.BUILTINS.createConstant(name = 'float',
    value = ca_type.Type(),
    type=builtins.TYPE_TYPE)
builtins.STR_TYPE = builtins.BUILTINS.createConstant(name = 'string',
    value = ca_type.Type(),
    type=builtins.TYPE_TYPE)
builtins.BOOL_TYPE = builtins.BUILTINS.createConstant(name = 'bool',
    value = ca_type.Type(),
    type=builtins.TYPE_TYPE)
builtins.ANY_TYPE = builtins.BUILTINS.createConstant(name = 'any',
    value = ca_type.Type(),
    type=builtins.TYPE_TYPE)
builtins.SUBROUTINE_TYPE = builtins.BUILTINS.createConstant(name = 'Subroutine',
    value = ca_type.Type(),
    type=builtins.TYPE_TYPE)

# Register these types
python_bridge.PYTHON_TYPE_TO_CIRCA[int] = builtins.INT_TYPE
python_bridge.PYTHON_TYPE_TO_CIRCA[float] = builtins.FLOAT_TYPE
python_bridge.PYTHON_TYPE_TO_CIRCA[str] = builtins.STR_TYPE
python_bridge.PYTHON_TYPE_TO_CIRCA[bool] = builtins.BOOL_TYPE
python_bridge.PYTHON_TYPE_TO_CIRCA[ca_function.Function] = builtins.FUNC_TYPE
python_bridge.PYTHON_TYPE_TO_CIRCA[code.SubroutineDefinition] = builtins.SUBROUTINE_TYPE

# Create basic constants
builtins.BUILTINS.createConstant(name='true', value=True, type=builtins.BOOL_TYPE)
builtins.BUILTINS.createConstant(name='false', value=False, type=builtins.BOOL_TYPE)

# Load builtins.ca file into this code unit
def installLibFile(filename):
    file = open(os.path.join(CIRCA_HOME, "lib", filename), 'r')
    file_contents = file.read()
    file.close()
    del file
    parser.parse(parser.builder.Builder(target=builtins.BUILTINS),
            token.tokenize(file_contents), raise_errors=True)
installLibFile("builtins.ca")

# Access some objects were created in builtins.ca (and which need to be made available
# to Python code)
builtins.TOKEN_FUNC = builtins.BUILTINS.getNamedTerm("token")
builtins.OPERATOR_FUNC = builtins.BUILTINS.getNamedTerm("operator")
builtins.ASSIGN_OPERATOR_FUNC = builtins.BUILTINS.getNamedTerm("assign_operator")
builtins.INVOKE_SUB_FUNC = builtins.BUILTINS.getNamedTerm("invokeSubroutine")

# Fill in definitions for all builtin functions
def installFunc(name, func):
    wrappedFunc = python_bridge.wrapPythonFuncToEvaluate(func)
    targetTerm = builtins.BUILTINS.getNamedTerm(name)

    # Make sure nothing else has been installed
    if targetTerm.pythonValue.pythonEvaluate is not parser.PLACEHOLDER_FUNC_FOR_BUILTINS:
        raise Exception("Term " + name + " already has a builtin function installed")

    targetTerm.pythonValue.pythonEvaluate = wrappedFunc

def tokenEvaluate(s):
    return token.definitions.STRING_TO_TOKEN[s]
installFunc("token", tokenEvaluate)

def printEvaluate(s): print s
def getInputEvaluate():
    return raw_input("> ")
def assertEvaluate(b): 
    if not b:
        print "Assertion failure!"
def equalsEvaluate(a,b): return a == b
def nequalsEvaluate(a,b): return a != b
def addEvaluate(a,b): return a + b
def subEvaluate(a,b): return a - b
def multEvaluate(a,b): return a * b
def divEvaluate(a,b): return a / b
def breakEvaluate(a,b): pdb.set_trace()
def emptyFunc(): pass
installFunc("print", printEvaluate)
installFunc("get_input", getInputEvaluate)
installFunc("assert", assertEvaluate)
installFunc("equals", equalsEvaluate)
installFunc("not_equals", nequalsEvaluate)
installFunc("add", addEvaluate)
installFunc("sub", subEvaluate)
installFunc("mult", multEvaluate)
installFunc("div", divEvaluate)
installFunc("break", breakEvaluate)
installFunc("invokeSubroutine", emptyFunc)

# Read in parsing.ca
installLibFile("parsing.ca")

