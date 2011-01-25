// Copyright (c) Paul Hodge. See LICENSE file for license terms.

#include <circa.h>

#include "gl_util.h"
#include "plastic_common_headers.h"

void gl_clear_error()
{
    glGetError();
}

const char* gl_check_error()
{
    GLenum errcode = glGetError();
    if (errcode == GL_NO_ERROR)
        return NULL;

    return gl_to_string(errcode);
}

bool gl_check_error(circa::EvalContext* cxt, circa::Term* term)
{
    const char* err = gl_check_error();

    if (err != NULL) {
        circa::error_occurred(cxt, term, std::string("OpenGL reported error: ") + err);
        return true;
    }

    return false;
}

const char* gl_to_string(GLenum glenum)
{
    switch (glenum) {
        case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
        case GL_STACK_OVERFLOW: return "GL_STACK_OVERFLOW";
        case GL_STACK_UNDERFLOW: return "GL_STACK_UNDERFLOW";
        case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
        case GL_RGBA: return "GL_RGBA";
        case GL_BGRA: return "GL_BGRA";

        #ifndef PLASTIC_OGL_ES
        case GL_RGB: return "GL_RGB";
        case GL_BGR: return "GL_BGR";
        #endif
    }
    return "";
}
