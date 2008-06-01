
from string import Template
from Circa.core import (builtins, ca_type, ca_function)
from Circa import (debug, parser)


class Browser(object):
    def __init__(self, codeUnit=None):
        self.codeUnit = codeUnit
    def doCommand(self, cmd, args):
        if cmd == 'list' or cmd == 'l':
            for term in self.codeUnit.allTerms:
                print self.describeTerm(term, "$id: $func_name($input_ids)")

        elif cmd == 'details':
            term = self.getTermFromIdentifier(args[0])
            if term is None:
                print "Couldn't find '%s'" % args[0]

            print self.describeTerm(term, "$id: $func_name($input_ids) = $value")

        elif cmd == 'create' or cmd == 'c':
            ast = parser.parseExpression(args)
            print ast
            ast.eval(self.codeUnit)

        else:
            print "Unrecognized command: " + cmd
    def getIdentifier(self, term):
        return self.codeUnit.getIdentifier(term)

    def getTermFromIdentifier(self, id):
        debug._assert(isinstance(id, str))

        # Handle ids of the form '#xxx'
        if id[0] == '#':
            index = int(id[1:])

            for term in self.codeUnit.allTerms:
                if term.globalID == index:
                    return term
            return None

        if id in self.codeUnit.mainNamespace:
            return self.codeUnit.mainNamespace[id]

        return None

    def describeTerm(self, term, template):
        """
        Valid tokens in 'template' are:
          $id : the term's identifier
          $func_id : ID of the term's function
          $input_ids : ID of the term's inputs, comma seperated
          $value : the term's current value
        """
        return Template(template).safe_substitute(
                  { 'id': self.getIdentifier(term),
                    'func_id': self.getIdentifier(term.functionTerm),
                    'func_name': ca_function.name(term.functionTerm),
                    'input_ids': ','.join(map(self.getIdentifier,term.inputs)),
                    'value': str(term),
                    'type': ca_type.name(term.getType()) })

def parseCommand(string):
    """
    Parse a string into a command, followed by a space, followed by
    whatever else. Returns tuple (command, whatever_else)
    """

    first_space = string.find(' ')
    if first_space == -1:
        return (string, "")
    else:
        return (string[:first_space], string[first_space+1:])

def main():
    import Circa.core.bootstrap

    browser = Browser(builtins.KERNEL)
    while True:
        try:
            userInput = raw_input('> ')
        except KeyboardInterrupt:
            print ""
            return

        if userInput == 'exit':
            return

        (command, options) = parseCommand(userInput)
        browser.doCommand(command, options)

if __name__ == "__main__":
    main()
