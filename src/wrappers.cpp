// Copyright 2008 Andrew Fischer

#include "branch.h"
#include "cpp_importing.h"
#include "values.h"

namespace circa {

std::string read_text_file(std::string const& filename)
{
    Branch workspace;
    string_value(workspace, filename, "filename");
    return eval_as<std::string>(workspace, "read-text-file(filename)");
}
    
} // namespace circa
