// Copyright (c) Andrew Fischer. See LICENSE file for license terms.

#include "circa/internal/for_hosted_funcs.h"

#include "file.h"

namespace circa {
namespace write_text_file_function {

    CA_FUNCTION(evaluate)
    {
        write_text_file(STRING_INPUT(0), STRING_INPUT(1));
    }

    void setup(Block* kernel)
    {
        import_function(kernel, evaluate,
            "write_text_file(String filename, String contents);"
            "'Write contents to the given filename, overwriting any existing file'");
    }
}
} // namespace circa
