
from Circa.common import (debug,errors)

class TermSyntaxInfo(object):
    def __str__(self):
        raise errors.PureVirtualMethodFail(self, '__str__')

class HighLevelOption(TermSyntaxInfo):
    def __init__(self, optionName):
        self.optionName = optionName
    def __str__(self):
        return '[' + self.optionName + ']'

class Expression(TermSyntaxInfo):
    def __init__(self, expr, nameBinding=None):
        self.nameBinding = nameBinding
        self.expressionAst = expr
    def __str__(self):
        result = ""
        if self.nameBinding is not None:
            result += self.nameBinding + ' = '
        result += str(self.expressionAst)
        return result

class CommentLine(TermSyntaxInfo):
    def __init__(self, text):
        self.text = text
        pass
    def __str__(self):
        return self.text

class Node(object):
    def __str__(self):
        raise errors.PureVirtualMethodFail(self, '__str__')

class FunctionCall(Node):
    def __init__(self, functionName, args):
        self.functionName = functionName
        self.args = args
    def __str__(self):
        return (self.functionName + '(' + ', '.join(map(str,self.args)) + ')')

class Infix(Node):
    def __init__(self, operator, left, right):
        self.operator = operator
        self.left = left
        self.right = right
    def __str__(self):
        return str(self.left) + ' ' + self.operator + ' ' + str(self.right)

class Unary(Node):
    def __init__(self, operator, right):
        self.operator = operator
        self.right = right
    def __str__(self):
        return self.operator + str(right)

class TermName(Node):
    def __init__(self, term, name):
        self.term = term
        self.name = name
    def __str__(self):
        return self.name

class TermValue(Node):
    def __init__(self, term):
        self.term = term
    def __str__(self):
        return str(self.term)
