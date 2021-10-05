#include "DrawText.hpp"

#include <iostream>
#include <hb.h>
#include <hb-ft.h>
#include <glm/gtc/matrix_transform.hpp>

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"


DrawText::DrawText(const char *font_path) {
    /**
     * Freetype initialization codes come from its tutorial
     * https://www.freetype.org/freetype2/docs/tutorial/step1.html
     */
    FT_Error error;
    error = FT_Init_FreeType(&library);
    if (error) {
        std::cerr << "Freetype initialization failed." << std::endl;
    }
    error = FT_New_Face(library, font_path, 0, &face_ft);
    if (error == FT_Err_Unknown_File_Format) {
        std::cerr << "Font format not supported." << std::endl;
    } else if (error) {
        std::cerr << "Font file is broken." << std::endl;
    }

    // OpenGL create related vertex data buffer. Every char is a rectangle
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, 6 * 4 * sizeof(GLfloat), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);  // unbind
    glBindVertexArray(0);

    // shader adapted from https://learnopengl.com/In-Practice/Text-Rendering
    program = gl_compile_program(
        "#version 330 core\n"
        "layout (location=0) in vec4 vertex;\n"  // [posx, posy, texx, texy]
        "out vec2 TexCoords;\n"
        "\n"
        "uniform mat4 projection;\n"
        "\n"
        "void main() {\n"
        "  gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);\n"
        "  TexCoords = vertex.zw;\n"
        "}\n"
        ,
        "#version 330 core\n"
        "in vec2 TexCoords;\n"
        "out vec4 color;\n"
        "\n"
        "uniform sampler2D text;\n"
        "uniform vec3 textColor;\n"
        "\n"
        "void main() {\n"
        "  vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);\n"
        "  color = vec4(textColor, 1.0) * sampled;\n"
        "}\n"
    );
    text_color_loc = glGetUniformLocation(program, "textcolor");
    projection_loc = glGetUniformLocation(program, "projection");
}

DrawText::~DrawText() {
    delete_cache();
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    FT_Done_Face(face_ft);
    FT_Done_FreeType(library);
}

void DrawText::set_font_size(int pixels) {
    // clear outdated cache
    delete_cache();
    
    FT_Error error = FT_Set_Pixel_Sizes(face_ft, pixels, 0);
    if (error) {
        std::cerr << "Font size setting failed." << std::endl;
    }
}

void DrawText::set_font_color(float r, float g, float b) {
    text_color.r = r;
    text_color.g = g;
    text_color.b = b;
}

void DrawText::set_window_size(float w, float h) {
    projection = glm::ortho(0.f, w, 0.f, h);  // origin in bottom-left
}

void DrawText::delete_cache() {
    texture_cache.clear();
}

GLuint DrawText::add_texture(FT_ULong codepoint) {
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);  // disable alignment
    if (FT_Load_Glyph(face_ft, codepoint, FT_LOAD_RENDER) != 0) {
        std::cerr << "Freetype char render error" << std::endl;
        return 0U;
    }
    FT_Bitmap &bitmap = face_ft->glyph->bitmap;
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RED,  // grayscale
        bitmap.width,
        bitmap.rows,
        0,
        GL_RED,
        GL_UNSIGNED_BYTE,
        bitmap.buffer
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // store cache
    texture_cache.emplace(
        std::piecewise_construct, 
        std::make_tuple(codepoint), 
        std::make_tuple(
            texture, 
            bitmap.width, 
            bitmap.rows,
            face_ft->glyph->bitmap_left,
            face_ft->glyph->bitmap_top
            )
        );

    // unbind texture
    glBindTexture(GL_TEXTURE_2D, 0);

    return texture;
}

void DrawText::draw(const char *text, float x, float y) {
    // The draw codes are adapted from the following references
    // https://github.com/harfbuzz/harfbuzz-tutorial/blob/master/hello-harfbuzz-freetype.c
    // https://learnopengl-cn.github.io/06%20In%20Practice/02%20Text%20Rendering/
    hb_font_t *hb_font;
    hb_font = hb_ft_font_create(face_ft, nullptr);

    // create hb buffer
    hb_buffer_t *hb_buffer;
    hb_buffer = hb_buffer_create();
    hb_buffer_add_utf8(hb_buffer, text, -1, 0, -1);
    hb_buffer_guess_segment_properties(hb_buffer);

    // shape it
    hb_shape(hb_font, hb_buffer, nullptr, 0);

    // retrieve info
    unsigned int len = hb_buffer_get_length(hb_buffer);
    hb_glyph_info_t *info = hb_buffer_get_glyph_infos(hb_buffer, nullptr);
    hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(hb_buffer, nullptr);

    // render
    glUseProgram(program);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUniform3f(text_color_loc, text_color.r, text_color.g, text_color.b);
    glUniformMatrix4fv(projection_loc, 1, GL_FALSE, &projection[0].x);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    for (size_t i = 0; i < len; ++i) {
        FT_ULong glyph_id = info[i].codepoint;
        float x_advance = pos[i].x_advance / 64.f;
        float y_advance = pos[i].y_advance / 64.f;
        float x_offset  = pos[i].x_offset / 64.f;
        float y_offset  = pos[i].y_offset / 64.f;


        // look up texture
        auto it = texture_cache.find(glyph_id);
        if (it == texture_cache.end()) {
            add_texture(glyph_id);
            it = texture_cache.find(glyph_id);
        }
        auto& bitmap = it->second;

        GLfloat xpos = x + x_offset + bitmap.left, ypos = y + y_offset - bitmap.h + bitmap.top;

        GLfloat vertices[6][4] = {
            {xpos, ypos+bitmap.h, 0.f, 0.f},
            {xpos, ypos, 0.f, 1.f},
            {xpos+bitmap.w, ypos, 1.f, 1.f},
            {xpos, ypos+bitmap.h, 0.f, 0.f},
            {xpos+bitmap.w, ypos, 1.f, 1.f},
            {xpos+bitmap.w, ypos+bitmap.h, 1.f, 0.f}  
        };

        // bind texture
        glBindTexture(GL_TEXTURE_2D, bitmap.texture_id);

        // update vertices data
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        // draw call
        glDrawArrays(GL_TRIANGLES, 0, 6);

        x += x_advance;
        y += y_advance;
    }

    GL_ERRORS();

    // unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);

    // cleaning
    hb_buffer_destroy(hb_buffer);
    hb_font_destroy(hb_font);
}