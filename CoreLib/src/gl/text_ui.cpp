#include "text_ui.hpp"
#include <GL/glew.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <iostream>
#include <string>

#pragma region shader
using namespace vpg::gl;
using namespace std;

#ifdef __EMSCRIPTEN__
static const char* text_vs = R"(#version 300 es
precision highp float;
layout(location = 0) in vec4 a_pos_uv;
uniform mat4 u_proj;
out vec2 v_uv;
void main() {
    gl_Position = u_proj * vec4(a_pos_uv.xy, 0.0, 1.0);
    v_uv = a_pos_uv.zw;
}
)";

static const char* text_fs = R"(#version 300 es
precision highp float;
in vec2 v_uv;
uniform sampler2D u_tex;
uniform vec3 u_color;
out vec4 fragColor;
void main() {
    float a = texture(u_tex, v_uv).r;
    fragColor = vec4(u_color, a);
}
)";
#else
static const char* text_vs = R"(
#version 330 core
layout(location = 0) in vec4 a_pos_uv;
uniform mat4 u_proj;
out vec2 v_uv;
void main() {
    gl_Position = u_proj * vec4(a_pos_uv.xy, 0.0, 1.0);
    v_uv = a_pos_uv.zw;
}
)";

static const char* text_fs = R"(
#version 330 core
in vec2 v_uv;
uniform sampler2D u_tex;
uniform vec3 u_color;
out vec4 fragColor;
void main() {
    float a = texture(u_tex, v_uv).r;
    fragColor = vec4(u_color, a);
}
)";
#endif
#pragma endregion

TextUI::~TextUI() { shutdown(); }

bool TextUI::init(const char* font_path, int pixel_height) {
    FT_Library ft_lib = nullptr;
    FT_Face ft_face = nullptr;

    if (FT_Init_FreeType(&ft_lib)) {
        std::cerr << "TextUI: FT_Init_FreeType failed\n";
        return false; 
    }
    ft = ft_lib;

    if (FT_New_Face(ft_lib, font_path, 0, &ft_face)) {
        std::cerr << "TextUI: could not load font: " << font_path << "\n";
        return false;
    }
    face = ft_face;

    FT_Set_Pixel_Sizes(ft_face, 0, (FT_UInt)pixel_height);

    // Shader
    auto compile_shader = [](GLenum type, const char* src) -> GLuint {
        GLuint sh = glCreateShader(type);
        glShaderSource(sh, 1, &src, nullptr);
        glCompileShader(sh);
        GLint ok = 0;
        glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[1024]{};
            glGetShaderInfoLog(sh, 1023, nullptr, log);
            std::cerr << "TextUI shader compile failed:\n" << log << "\n";
            glDeleteShader(sh);
            return 0;
        }
        return sh;
    };

    GLuint vs = compile_shader(GL_VERTEX_SHADER, text_vs);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, text_fs);
    if (vs == 0 || fs == 0) {
        return false;
    }

    shader_prog = glCreateProgram();
    glAttachShader(shader_prog, vs);
    glAttachShader(shader_prog, fs);
    glLinkProgram(shader_prog);
    {
        GLint ok = 0;
        glGetProgramiv(shader_prog, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[1024]{};
            glGetProgramInfoLog(shader_prog, 1023, nullptr, log);
            std::cerr << "TextUI shader link failed:\n" << log << "\n";
            glDeleteShader(vs);
            glDeleteShader(fs);
            return false;
        }
    }
    glDeleteShader(vs);
    glDeleteShader(fs);

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    return true;
}

void TextUI::shutdown() {
    for (auto& p : glyphs) {
        glDeleteTextures(1, &p.second.texture_id);
    }
    glyphs.clear();
    if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
    if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
    if (face) { FT_Done_Face((FT_Face)face); face = nullptr; }
    if (ft)  { FT_Done_FreeType((FT_Library)ft); ft = nullptr; }
}

void TextUI::load_glyph(char c) {
    if (glyphs.count(c)) return;
    FT_Face f = (FT_Face)face;
    if (FT_Load_Char(f, (FT_ULong)(unsigned char)c, FT_LOAD_RENDER))
        return;

    FT_GlyphSlot g = f->glyph;
    int w = (int)g->bitmap.width;
    int h = (int)g->bitmap.rows;
    if (w == 0 || h == 0) {
        glyphs[c] = { 0, 0, 0, (int)g->bitmap_left, (int)g->bitmap_top, (long)g->advance.x };
        return;
    }

    unsigned int tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    GLint prev_unpack = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &prev_unpack);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
#ifdef __EMSCRIPTEN__
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, g->bitmap.buffer);
#else
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, g->bitmap.buffer);
#endif
    glPixelStorei(GL_UNPACK_ALIGNMENT, prev_unpack);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    glyphs[c] = { tex, w, h, (int)g->bitmap_left, (int)g->bitmap_top, (long)g->advance.x };
}

void TextUI::use_ortho(int w, int h) {
    // Ortho: left=0, right=w, bottom=0, top=h (y up in pixels)
    float L = 0.f, R = (float)w, B = 0.f, T = (float)h;
    float proj[16] = {
        2.f/(R-L), 0, 0, 0,
        0, 2.f/(T-B), 0, 0,
        0, 0, -1, 0,
        -(R+L)/(R-L), -(T+B)/(T-B), 0, 1
    };
    glUniformMatrix4fv(glGetUniformLocation(shader_prog, "u_proj"), 1, GL_FALSE, proj);
}

void TextUI::render(const std::string& text, float x, float y, float scale, glm::vec3 color, int viewport_width, int viewport_height) {
    if (!face || !shader_prog) return;
    //std::reverse(text.begin(), text.end()); "no matching overloaded function found"
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(shader_prog);
    use_ortho(viewport_width, viewport_height);
    glUniform3fv(glGetUniformLocation(shader_prog, "u_color"), 1, &color[0]);
    glBindVertexArray(vao);

    float cursor_x = x;
    float cursor_y = y;

    for (char c : text) {
        if ((unsigned char)c < 32) continue;
        load_glyph(c);

        auto it = glyphs.find(c);
        if (it == glyphs.end()) continue;
        const auto& g = it->second;
        if (g.texture_id == 0) {
            cursor_x += (float)(g.advance >> 6) * scale;
            continue;
        }

        //here to verts is just calculating where the text be
        float x2 = cursor_x + (float)g.bearing_x * scale;
        float y2 = cursor_y + (float)g.bearing_y * scale;
        float w = (float)g.width * scale;
        float h = (float)g.height * scale;
        float verts[24] = {
            x2,     y2 - h, 0, 1,
            x2 + w, y2 - h, 1, 1,
            x2 + w, y2,     1, 0,
            x2,     y2 - h, 0, 1,
            x2 + w, y2,     1, 0,
            x2,     y2,     0, 0
            
        };
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, nullptr);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g.texture_id);
        glUniform1i(glGetUniformLocation(shader_prog, "u_tex"), 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        cursor_x += (float)(g.advance >> 6) * scale;
    }

    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}



