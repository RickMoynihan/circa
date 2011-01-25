// Copyright (c) Paul Hodge. See LICENSE file for license terms.

#include <circa.h>

#include "plastic_common_headers.h"

#include "app.h"
#include "plastic_main.h"

GLuint load_shader(std::string const& vertFilename, std::string const& fragFilename)
{
    std::string vertText = circa::storage::read_text_file_as_str(
            app::find_asset_file(vertFilename).c_str());
    std::string fragText = circa::storage::read_text_file_as_str(
            app::find_asset_file(fragFilename).c_str());

    const char* vert = vertText.c_str();
    const char* frag = fragText.c_str();
    
    GLchar buf[256];
    GLuint vertShader, fragShader, program;
    GLint success;

    vertShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertShader, 1, (const GLchar**) &vert, 0);
    glCompileShader(vertShader);
    glGetShaderiv(vertShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertShader, sizeof(buf), 0, buf);
        app::error("Unable to compile vertex shader.");
    }

    fragShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragShader, 1, (const GLchar**) &frag, 0);
    glCompileShader(fragShader);
    glGetShaderiv(fragShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragShader, sizeof(buf), 0, buf);
        app::error("Unable to compile fragment shader.");
    }

    program = glCreateProgram();
    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);

    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(program, sizeof(buf), 0, buf);
        app::error("Unable to link shaders.\n");
    }

    return program;
}
