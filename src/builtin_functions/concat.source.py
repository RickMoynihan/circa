# Copyright 2008 Andrew Fischer

header = "function concat(string,string) -> string"
pure = True
evaluate = """
    as_string(caller) = as_string(caller->inputs[0]) + as_string(caller->inputs[1]);
"""
