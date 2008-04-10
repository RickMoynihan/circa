
import string, os, pdb

from Circa import (
    builtins,
    builtin_functions,
    containers,
    code,
    ca_type,
    ca_function,
    debug,
    parser,
    python_bridge,
    signature,
    token
)

# fixme
CIRCA_HOME = "/Users/andyfischer/code/circa"

VERBOSE_DEBUGGING = False

builtins.BUILTINS = code.CodeUnit()

# Aliases for creating terms on builtins.BUILTINS
createTerm = builtins.BUILTINS.createTerm
createConstant = builtins.BUILTINS.createConstant

# Create constant-generator function, a function which spits out constant functions.
# This term is termporarily incomplete, since he requires a few circular references.
builtins.CONST_FUNC_GENERATOR = code.Term()
builtins.CONST_FUNC_GENERATOR.codeUnit = builtins.BUILTINS
builtins.CONST_FUNC_GENERATOR.functionTerm = builtins.CONST_FUNC_GENERATOR
builtins.CONST_FUNC_GENERATOR.globalID = 1
ca_function.setValue(builtins.CONST_FUNC_GENERATOR, name="constant-generator")

# Create 'constant-Type' function
builtins.CONST_TYPE_FUNC = createTerm(functionTerm=builtins.CONST_FUNC_GENERATOR)
ca_function.setValue(builtins.CONST_TYPE_FUNC)

# Create Type type
builtins.TYPE_TYPE = createTerm(functionTerm=builtins.CONST_TYPE_FUNC, name = 'Type')

# Implant the Type type
ca_function.setValue(builtins.CONST_FUNC_GENERATOR, inputs=[builtins.TYPE_TYPE])
ca_function.setValue(builtins.CONST_TYPE_FUNC, output=builtins.TYPE_TYPE)

# Create 'constant-Function' function
builtins.CONST_FUNC_FUNC = createTerm(functionTerm=builtins.CONST_FUNC_GENERATOR)
ca_function.setValue(builtins.CONST_FUNC_FUNC)

# Create Function type
builtins.FUNC_TYPE = createTerm(builtins.CONST_FUNC_FUNC, name = 'Function')

# Map Python type to this type
python_bridge.registerType(ca_function._Function, builtins.FUNC_TYPE)

# Implant types into 'constant' function, finish defining it
def constFuncGeneratorEval(term):
   type = term.inputs[0]
   debugName = "constant-" + type.getSomeName()
   ca_function.setValue(term, inputs=[], output=type, name=debugName)

ca_function.setValue(builtins.CONST_FUNC_GENERATOR, inputs= [builtins.TYPE_TYPE],
   output= builtins.FUNC_TYPE, evaluateFunc=constFuncGeneratorEval)

# Create and register primitive types
builtins.INT_TYPE = builtins.BUILTINS.createConstant(name = 'int',
    value = ca_type.Type(),
    valueType=builtins.TYPE_TYPE)
python_bridge.registerType(int, builtins.INT_TYPE)

builtins.FLOAT_TYPE = builtins.BUILTINS.createConstant(name = 'float',
    value = ca_type.Type(),
    valueType=builtins.TYPE_TYPE)
python_bridge.registerType(float, builtins.FLOAT_TYPE)

builtins.STR_TYPE = builtins.BUILTINS.createConstant(name = 'string',
    value = ca_type.Type(),
    valueType=builtins.TYPE_TYPE)
python_bridge.registerType(str, builtins.STR_TYPE)

builtins.BOOL_TYPE = builtins.BUILTINS.createConstant(name = 'bool',
    value = ca_type.Type(),
    valueType=builtins.TYPE_TYPE)
python_bridge.registerType(bool, builtins.BOOL_TYPE)

builtins.REF_TYPE = builtins.BUILTINS.createConstant(name = 'Ref',
    value = ca_type.Type(),
    valueType=builtins.TYPE_TYPE)

builtins.SUBROUTINE_TYPE = builtins.BUILTINS.createConstant(name = 'Subroutine',
    value = ca_type.Type(),
    valueType=builtins.TYPE_TYPE)
python_bridge.registerType(code.SubroutineDefinition, builtins.SUBROUTINE_TYPE)

# Create basic constants
builtins.BUILTINS.createConstant(name='true', value=True, valueType=builtins.BOOL_TYPE)
builtins.BUILTINS.createConstant(name='false', value=False, valueType=builtins.BOOL_TYPE)

# Create Map function
builtins.MAP_GENERATOR = builtins.BUILTINS.createConstant(name = 'map',
      valueType=builtins.FUNC_TYPE)
ca_function.setValue(builtins.MAP_GENERATOR,
    inputs=[builtins.TYPE_TYPE, builtins.TYPE_TYPE],
    output=builtins.FUNC_TYPE,
    initFunc = builtin_functions.mapGeneratorInit,
    evaluateFunc = builtin_functions.mapGeneratorEval,
    hasState=True)

# Create Variable generator function
builtins.VARIABLE_FUNC_GENERATOR = builtins.BUILTINS.createConstant(
      valueType=builtins.FUNC_TYPE)


def _variableGenerator(term):
   type = term.inputs[0]
   ca_function.setValue(term, output=type,
         feedbackFunc=builtin_functions.variableFeedbackFunc,
         name="variable-" + type.getSomeName())

ca_function.setValue(builtins.VARIABLE_FUNC_GENERATOR,
         inputs=[builtins.TYPE_TYPE], output=builtins.FUNC_TYPE,
         evaluateFunc = _variableGenerator,
         name="variable-generator")

# Create feedback function


# Load builtins.ca file into this code unit
builtinsFilename = os.path.join(CIRCA_HOME, "lib", "builtins.ca")
parser.parseFile(parser.builder.Builder(target=builtins.BUILTINS),
         builtinsFilename, raise_errors=True,
         pythonObjectSource=builtin_functions.NAME_TO_FUNC)

# Expose some objects that were created in Circa code so that they may be accessed
# from Python code

def getCircaDefined(name):
   obj = builtins.BUILTINS.getNamedTerm(name)
   if obj is None: raise Exception("Couldn't find term named: " + name)
   return obj

builtins.TOKEN_FUNC = getCircaDefined("token")
builtins.OPERATOR_FUNC = getCircaDefined("operator")
builtins.ASSIGN_OPERATOR_FUNC = getCircaDefined("assign_operator")
builtins.ASSIGN_FUNC = getCircaDefined("assign")
builtins.ADD_FUNC = getCircaDefined("add")
builtins.SUBTRACT_FUNC = getCircaDefined("sub")
builtins.MULTIPLY_FUNC = getCircaDefined("mult")

# Install builtin functions into pre-existing Circa objects
def installFunc(name, value):
   targetTerm = builtins.BUILTINS.getNamedTerm(name)

   # Make sure nothing else has been installed
   if targetTerm.pythonValue.pythonEvaluate is not parser.PLACEHOLDER_EVALUATE_FOR_BUILTINS:
      raise Exception("Term " + name + " already has a builtin function installed")

   targetTerm.pythonValue.pythonInit = value.pythonInit
   targetTerm.pythonValue.pythonEvaluate = value.pythonEvaluate


