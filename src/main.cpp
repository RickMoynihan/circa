// Copyright (c) Paul Hodge. See LICENSE file for license terms.

#include "circa.h"

int main(int argc, const char * args[])
{
    circa_initialize();
    circa_storage_use_filesystem();

    int result = 0;
    result = circa_run_command_line(argc, args);

    circa_shutdown();
    
    return result;
}
