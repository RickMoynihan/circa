// Copyright (c) Andrew Fischer. See LICENSE file for license terms.

#include <cmath>
#include <cstdio>
#include <cairo/cairo.h>

#include <OpenGL/glu.h>
#include <OpenGL/glext.h>

#include "circa/circa.h"

cairo_t* as_cairo_context(caValue* value)
{
    return (cairo_t*) circa_handle_get_object(value);
}

cairo_surface_t* as_cairo_surface(caValue* value)
{
    return (cairo_surface_t*) circa_handle_get_object(value);
}

cairo_font_face_t* as_cairo_font_face(caValue* value)
{
    return (cairo_font_face_t*) circa_handle_get_object(value);
}

void Context_release(caStack* stack)
{
    cairo_t* context = as_cairo_context(circa_input(stack, 0));
    cairo_destroy(context);
}
void Surface_release(caStack* stack)
{
    cairo_surface_t* surface = as_cairo_surface(circa_input(stack, 0));
    printf("destroy surface: %p\n", surface);
    cairo_surface_destroy(surface);
}
void FontFace_release(caStack* stack)
{
    cairo_font_face_t* font = as_cairo_font_face(circa_input(stack, 0));
    cairo_font_face_destroy(font);
}

float radians_to_degrees(float radians) { return radians * 180.0 / M_PI; }
float degrees_to_radians(float unit) { return unit * M_PI / 180.0; }

void make_context(caStack* stack)
{
    cairo_surface_t* surface = as_cairo_surface(circa_input(stack, 0));
    cairo_t* context = cairo_create(surface);
    caValue* out = circa_create_default_output(stack, 0);
    circa_handle_set_object(out, context);
}
void Context_save(caStack* stack)
{
    cairo_t* context = as_cairo_context(circa_input(stack, 0));
    cairo_save(context);
}
void Context_restore(caStack* stack)
{
    cairo_t* context = as_cairo_context(circa_input(stack, 0));
    cairo_restore(context);
}

void Context_stroke(caStack* stack)
{
    cairo_t* context = as_cairo_context(circa_input(stack, 0));
    cairo_stroke(context);
}
void Context_fill(caStack* stack)
{
    cairo_t* context = as_cairo_context(circa_input(stack, 0));
    cairo_fill(context);
}
void Context_paint(caStack* stack)
{
    cairo_t* context = as_cairo_context(circa_input(stack, 0));
    cairo_paint(context);
    circa_set_null(circa_output(stack, 0));
}
void Context_clip(caStack* stack)
{
    cairo_t* context = as_cairo_context(circa_input(stack, 0));
    cairo_clip(context);
    circa_set_null(circa_output(stack, 0));
}
void Context_clip_preserve(caStack* stack)
{
    cairo_t* context = as_cairo_context(circa_input(stack, 0));
    cairo_clip_preserve(context);
    circa_set_null(circa_output(stack, 0));
}
void Context_reset_clip(caStack* stack)
{
    cairo_t* context = as_cairo_context(circa_input(stack, 0));
    cairo_reset_clip(context);
    circa_set_null(circa_output(stack, 0));
}
void Context_set_source_color(caStack* stack)
{
    cairo_t* context = as_cairo_context(circa_input(stack, 0));
    float r(0), g(0), b(0), a(0);
    circa_vec4(circa_input(stack, 1), &r, &g, &b, &a);
    cairo_set_source_rgba(context, r, g, b, a);
    circa_set_null(circa_output(stack, 0));
}
void Context_fill_preserve(caStack* stack)
{
    cairo_t* context = as_cairo_context(circa_input(stack, 0));
    cairo_fill_preserve(context);
}
void Context_set_operator(caStack* stack)
{
    cairo_t* context = as_cairo_context(circa_input(stack, 0));
    cairo_set_operator(context, (cairo_operator_t) circa_int_input(stack, 1));
}

void Context_select_font_face(caStack* stack)
{
    cairo_t* context = as_cairo_context(circa_input(stack, 0));
    const char* family = circa_string_input(stack, 1);
    // currently, user can't specify font slant or weight.
    cairo_select_font_face(context, family, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
}
void Context_set_font_size(caStack* stack)
{
    cairo_t* context = as_cairo_context(circa_input(stack, 0));
    cairo_set_font_size(context, circa_float_input(stack, 1));
}
void Context_set_font_face(caStack* stack)
{
    cairo_t* context = as_cairo_context(circa_input(stack, 0));
    cairo_font_face_t* font = as_cairo_font_face(circa_input(stack, 1));
    cairo_set_font_face(context, font);
}
void Context_show_text(caStack* stack)
{
    cairo_t* context = as_cairo_context(circa_input(stack, 0));
    const char* str = circa_string_input(stack, 1);
    cairo_show_text(context, str);
}
void Context_text_extents(caStack* stack)
{
    cairo_t* context = as_cairo_context(circa_input(stack, 0));
    cairo_text_extents_t extents;
    cairo_text_extents(context, circa_string_input(stack, 1), &extents);

    caValue* out = circa_create_default_output(stack, 0);
    circa_set_point(circa_index(out, 0), extents.x_bearing, extents.y_bearing);
    circa_set_point(circa_index(out, 1), extents.width, extents.height);
    circa_set_point(circa_index(out, 2), extents.x_advance, extents.y_advance);
}

void Context_curve_to(caStack* stack)
{
    cairo_t* context = as_cairo_context(circa_input(stack, 0));
    float x1(0), y1(0), x2(0), y2(0), x3(0), y3(0);
    circa_vec2(circa_input(stack, 1), &x1, &x2);
    circa_vec2(circa_input(stack, 2), &x1, &x2);
    circa_vec2(circa_input(stack, 3), &x1, &x2);
    cairo_curve_to(context, x1, y1, x2, y2, x3, y3);
    circa_set_null(circa_output(stack, 0));
}
void Context_move_to(caStack* stack)
{
    cairo_t* context = as_cairo_context(circa_input(stack, 0));
    float x(0), y(0);
    circa_vec2(circa_input(stack, 1), &x, &y);
    cairo_move_to(context, x, y);
}
void Context_rectangle(caStack* stack)
{
    cairo_t* context = as_cairo_context(circa_input(stack, 0));
    float x1(0), y1(0), x2(0), y2(0);
    circa_vec4(circa_input(stack, 1), &x1, &y1, &x2, &y2);
    cairo_rectangle(context, x1, y1, x2-x1, y2-y1);
}
void Context_line_to(caStack* stack)
{
    cairo_t* context = as_cairo_context(circa_input(stack, 0));
    float x(0), y(0);
    circa_vec2(circa_input(stack, 1), &x, &y);
    cairo_line_to(context, x, y);
}
void Context_rel_line_to(caStack* stack)
{
    cairo_t* context = as_cairo_context(circa_input(stack, 0));
    float x(0), y(0);
    circa_vec2(circa_input(stack, 1), &x, &y);
    cairo_rel_line_to(context, x, y);
}
void Context_arc(caStack* stack)
{
    cairo_t* context = as_cairo_context(circa_input(stack, 0));
    float center_x(0), center_y(0);
    circa_vec2(circa_input(stack, 1), &center_x, &center_y);
    cairo_arc(context, center_x, center_y,
        circa_float_input(stack, 2),
        degrees_to_radians(circa_float_input(stack, 3)),
        degrees_to_radians(circa_float_input(stack, 4)));
}
void Context_new_sub_path(caStack* stack)
{
    cairo_t* context = as_cairo_context(circa_input(stack, 0));
    cairo_new_sub_path(context);
}
void Context_close_path(caStack* stack)
{
    cairo_t* context = as_cairo_context(circa_input(stack, 0));
    cairo_close_path(context);
}
void Context_set_line_width(caStack* stack)
{
    cairo_t* context = as_cairo_context(circa_input(stack, 0));
    cairo_set_line_width(context, circa_to_float(circa_input(stack, 1)));
    circa_set_null(circa_output(stack, 0));
}
void make_image_surface(caStack* stack)
{
    float width, height;
    circa_vec2(circa_input(stack, 0), &width, &height);

    // user can't currently specify the format
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
        int(width), int(height));

    caValue* out = circa_create_default_output(stack, 0);
    circa_handle_set_object(out, surface);
}

void Surface_write_to_png(caStack* stack)
{
    cairo_surface_t* surface = as_cairo_surface(circa_input(stack, 0));
    const char* filename = circa_string_input(stack, 1);

    cairo_status_t status = cairo_surface_write_to_png(surface, filename);

    if (status != CAIRO_STATUS_SUCCESS) {
        circa_output_error(stack, "cairo_surface_write_to_png failed");
    }
}

void upload_surface_to_opengl(caStack* stack)
{
    cairo_surface_t* surface = as_cairo_surface(circa_input(stack, 0));
    int texture_id = circa_int_input(stack, 1);

    unsigned char* pixels = cairo_image_surface_get_data(surface);
    int width = cairo_image_surface_get_width(surface);
    int height = cairo_image_surface_get_height(surface);
    int stride = cairo_image_surface_get_stride(surface);
    const int channels = 4;

    // Modify 'surface' so that colors are in RGBA order instead of BGRA
    for (int y=0; y < height; y++) for (int x=0; x < width; x++) {
        size_t offset = y * stride + x * channels;
        //unsigned char alpha = pixels[offset + 3];
        unsigned char b = pixels[offset + 0];
        unsigned char g = pixels[offset + 1];
        unsigned char r = pixels[offset + 2];
        unsigned char a = pixels[offset + 3];

        pixels[offset+0] = r;
        pixels[offset+1] = g;
        pixels[offset+2] = b;
        pixels[offset+3] = a;
    }

    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
            width, height, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    glBindTexture(GL_TEXTURE_2D, 0);

    circa_set_null(circa_output(stack, 0));
}

CIRCA_EXPORT void circa_module_load(caNativePatch* module)
{
    circa_patch_function(module, "Context.release", Context_release);
    circa_patch_function(module, "Context.save", Context_save);
    circa_patch_function(module, "Context.restore", Context_restore);
    circa_patch_function(module, "Context.stroke", Context_stroke);
    circa_patch_function(module, "Context.fill", Context_fill);
    circa_patch_function(module, "Context.paint", Context_paint);
    circa_patch_function(module, "Context.clip", Context_clip);
    circa_patch_function(module, "Context.clip_preserve", Context_clip_preserve);
    circa_patch_function(module, "Context.reset_clip", Context_reset_clip);
    circa_patch_function(module, "Context.set_source_color", Context_set_source_color);
    circa_patch_function(module, "Context.fill_preserve", Context_fill_preserve);
    circa_patch_function(module, "Context.set_operator", Context_set_operator);
    circa_patch_function(module, "Context.select_font_face", Context_select_font_face);
    circa_patch_function(module, "Context.set_font_size", Context_set_font_size);
    circa_patch_function(module, "Context.set_font_face", Context_set_font_face);
    circa_patch_function(module, "Context.show_text", Context_show_text);
    circa_patch_function(module, "Context.text_extents", Context_text_extents);
    circa_patch_function(module, "Context.curve_to", Context_curve_to);
    circa_patch_function(module, "Context.move_to", Context_move_to);
    circa_patch_function(module, "Context.rectangle", Context_rectangle);
    circa_patch_function(module, "Context.line_to", Context_line_to);
    circa_patch_function(module, "Context.rel_line_to", Context_rel_line_to);
    circa_patch_function(module, "Context.arc", Context_arc);
    circa_patch_function(module, "Context.new_sub_path", Context_new_sub_path);
    circa_patch_function(module, "Context.close_path", Context_close_path);
    circa_patch_function(module, "Context.set_line_width", Context_set_line_width);
    circa_patch_function(module, "make_image_surface", make_image_surface);
    circa_patch_function(module, "Surface.write_to_png", Surface_write_to_png);
    circa_patch_function(module, "Surface.release", Surface_release);
    circa_patch_function(module, "Surface.make_context", make_context);
    circa_patch_function(module, "FontFace.release", FontFace_release);
}
