// Copyright (c) Andrew Fischer. See LICENSE file for license terms.

#include "common_headers.h"

#include "building.h"
#include "function.h"
#include "inspection.h"
#include "list.h"
#include "names.h"
#include "names.h"
#include "string_type.h"
#include "type.h"

#include "generate_cpp.h"

namespace circa {

void repeat_string(const char* str, int times, caValue* output)
{
    std::stringstream out;
    for (int i=0; i < times; i++)
        out << str;
    set_string(output, out.str());
}

const int kSpacesPerIndent = 4;

struct SourceWriter
{
    List output;
    int currentIndent;
    bool startNewLine;
    Value indentStr;

    SourceWriter() : currentIndent(0), startNewLine(true) {
        repeat_string(" ", currentIndent * kSpacesPerIndent, &indentStr);
    }

    void possiblyStartNewLine()
    {
        if (startNewLine) {
            startNewLine = false;
            caValue* space = list_append(&output);
            copy(&indentStr, space);
        }
    }

    void write(caValue* item)
    {
        possiblyStartNewLine();

        if (string_eq(item, "\n")) {
            set_string(output.append(), "\n");
            startNewLine = true;
            return;
        }

        copy(item, output.append());
    }

    void write(const char* str)
    {
        possiblyStartNewLine();
        set_string(output.append(), str);
    }

    void newline()
    {
        Value val;
        set_string(&val, "\n");
        write(&val);
    }

    void writeList(caValue* list)
    {
        possiblyStartNewLine();
        int listLength = list_length(list);
        for (int i=0; i < listLength; i++) {
            caValue* item = list_get(list, i);
            copy(item, list_append(&output));
        }
    }
    void indent()
    {
        currentIndent++;
        repeat_string(" ", currentIndent * kSpacesPerIndent, &indentStr);
    }
    void unindent()
    {
        currentIndent--;
        repeat_string(" ", currentIndent * kSpacesPerIndent, &indentStr);
    }
};

void write_block_contents(SourceWriter* writer, Block* block);

void write_term_value(SourceWriter* writer, Term* term)
{
    caValue* val = term_value(term);
    if (is_int(val)) {
        writer->write(as_cstring(val));
    } else if (is_float(val)) {
        writer->write(as_cstring(val));
    } else if (is_string(val)) {
        writer->write("\"");
        writer->write(as_cstring(val));
        writer->write("\"");
    }
}

void write_type_name(SourceWriter* writer, Type* type)
{
    writer->write(as_cstring(&type->name));
}

void write_function(SourceWriter* writer, Term* term)
{
    Function* func = as_function(term);
    write_type_name(writer, function_get_output_type(func, 0));

    writer->write(term->name.c_str());

    int inputCount = count_input_placeholders(function_contents(term));

    writer->write("(");

    for (int i=0; i < inputCount; i++) {
        if (i > 0)
            writer->write(", ");

        Term* placeholder = function_get_input_placeholder(func, i);
        write_type_name(writer, placeholder->type);
        writer->write(" ");
        writer->write(get_unique_name(placeholder));
    }

    writer->write(")");
    writer->newline();
    writer->write("{");
    writer->indent();
    writer->unindent();
    writer->write("}");
    writer->newline();
}

void write_term(SourceWriter* writer, Term* term)
{
    if (is_comment(term)) {
        if (is_empty_comment(term))
            return;

        writer->write("// ");
        writer->write(term->stringProp("comment","").c_str());
        writer->newline();
        return;
    }

    if (is_function(term)) {
        write_function(writer, term);
        return;
    }

    writer->write(get_unique_name(term));
    writer->write(" = ");
    
    if (is_value(term)) {
        write_term_value(writer, term);
    } else {
        // function call syntax
        writer->write(term->function->name.c_str());
        writer->write("(");

        // write inputs
        for (int i=0; i < term->numInputs(); i++) {
            if (i > 0) {
                writer->write(", ");
            }
            writer->write(get_unique_name(term));
        }
    }

    writer->write(";");
    writer->newline();
}

void write_block_contents(SourceWriter* writer, Block* block)
{
    for (int i=0; i < block->length(); i++) {
        Term* term = block->get(i);
        if (is_input_placeholder(term) || is_output_placeholder(term))
            continue;
        write_term(writer, term);
    }
}

void write_program(Block* block, caValue* out)
{
    SourceWriter sourceWriter;
    write_block_contents(&sourceWriter, block);
    swap(&sourceWriter.output, out);
}

void write_program_to_file(Block* block, const char* filename)
{
    Value strs;
    write_program(block, &strs);

    FILE* file = fopen(filename, "w");

    int str_count = list_length(&strs);
    for (int i=0; i < str_count; i++) {
        caValue* str = list_get(&strs, i);
        const char* cstr = as_cstring(str);
        fwrite(cstr, 1, strlen(cstr), file);
    }
    fclose(file);
}

void run_generate_cpp(caValue* args)
{
    if (list_length(args) < 2) {
        std::cout << "Expected 2 arguments";
        return;
    }

    const char* source_file = as_cstring(list_get(args, 0));
    const char* output_file = as_cstring(list_get(args, 1));

    std::cout << "Loading source from: " << source_file << std::endl;
    std::cout << "Will write to: " << output_file << std::endl;
    
    Block block;
    load_script(&block, source_file);
    write_program_to_file(&block, output_file);
}

} // namespace circa
