#include <GL/glew.h>
#include "image_ui.hpp"

#include <iostream>
#include <algorithm>

using namespace vpg::gl;
static const char* img_vs = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aTexCoord;

out vec2 TexCoord;

void main()
{
    gl_Position = vec4(aPos, 1.0);
    TexCoord    = aTexCoord;
}
)";

static const char* img_fs = R"(
#version 330 core
in  vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D ourTexture;

void main()
{
    FragColor = texture(ourTexture, TexCoord);
}
)";

ImageUI::~ImageUI() { shutdown(); }

bool ImageUI::init()
{
    // ---- compile shaders ------------------------------------------------
    auto compile = [](GLenum type, const char* src) -> unsigned int {
        unsigned int id = glCreateShader(type);
        glShaderSource(id, 1, &src, nullptr);
        glCompileShader(id);
        int ok; glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[512]; glGetShaderInfoLog(id, 512, nullptr, log);
            std::cerr << "ImageUI shader compile error:\n" << log << "\n";
        }
        return id;
        };

    unsigned int vs = compile(GL_VERTEX_SHADER, img_vs);
    unsigned int fs = compile(GL_FRAGMENT_SHADER, img_fs);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vs);
    glAttachShader(shaderProgram, fs);
    glLinkProgram(shaderProgram);
    {
        int ok; glGetProgramiv(shaderProgram, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[512]; glGetProgramInfoLog(shaderProgram, 512, nullptr, log);
            std::cerr << "ImageUI program link error:\n" << log << "\n";
            return false;
        }
    }
    glDeleteShader(vs);
    glDeleteShader(fs);

    // ---- shared geometry ------------------------------------------------
    // Placeholder vertices; DrawImage overwrites them via glBufferSubData.
    // Layout per vertex (8 floats): x y z  r g b  u v
    float vertices[4 * 8] = {};

    unsigned int indices[] = {
        0, 1, 3,   // first  triangle
        1, 2, 3    // second triangle
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // aPos      – location 0, 3 floats
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // aColor    – location 1, 3 floats
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // aTexCoord – location 2, 2 floats
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    return true;
}

// ---------------------------------------------------------------------------
// load_texture – upload one image and return its integer ID.
// ---------------------------------------------------------------------------
int ImageUI::load_texture(int width, int height, unsigned char* data)
{
    if (!data) {
        std::cerr << "ImageUI::load_texture: pixel data is null.\n";
        return -1;
    }

    unsigned int tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height,
        0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    // The index into the vector becomes the public ID.
    int id = static_cast<int>(textures.size());
    textures.push_back(tex);
    return id;
}

void ImageUI::DrawImage(int texture_id, float crop_percent, float x1, float y1, float w1)
{
    if (texture_id < 0 || texture_id >= static_cast<int>(textures.size())) {
        std::cerr << "ImageUI::DrawImage: invalid texture_id " << texture_id << "\n";
        return;
    }

    crop_percent = std::max(0.0f, std::min(1.0f, crop_percent));

    // The quad spans the full screen width at the bottom; its right edge and
    // the UV's right edge both shrink by crop_percent ? crop, not stretch.
    const float BAR_HEIGHT_NDC = 0.07f;

    const float x_lft = x1; 
    const float y_top = y1 - BAR_HEIGHT_NDC;
    const float x_rgt = x1 + w1 * crop_percent;
    const float y_bot = y1;
    /*
    const float x_lft = -1.0f; 
    const float y_top = 1.0f - BAR_HEIGHT_NDC;
    const float x_rgt = -1.0f + .5f * crop_percent; 
    const float y_bot = 1.0f;
    */

    const float u_lft = 0.0f;
    const float u_rgt = crop_percent;

    // 4 vertices × 8 floats  (x y z  r g b  u v)
    float vertices[] = {
        x_rgt, y_top, 0.0f,  1.0f, 1.0f, 1.0f,  u_rgt, 1.0f,  // top-right
        x_rgt, y_bot, 0.0f,  1.0f, 1.0f, 1.0f,  u_rgt, 0.0f,  // bottom-right
        x_lft, y_bot, 0.0f,  1.0f, 1.0f, 1.0f,  u_lft, 0.0f,  // bottom-left
        x_lft, y_top, 0.0f,  1.0f, 1.0f, 1.0f,  u_lft, 1.0f,  // top-left
    };

    // Save relevant GL state
    GLboolean depth_test = glIsEnabled(GL_DEPTH_TEST);
    GLboolean cull_face = glIsEnabled(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    // Upload updated quad vertices
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

    // Bind the requested texture and draw
    glUseProgram(shaderProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures[texture_id]);
    glUniform1i(glGetUniformLocation(shaderProgram, "ourTexture"), 0);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
    glUseProgram(0);

    // Restore state
    if (depth_test) glEnable(GL_DEPTH_TEST);
    if (cull_face)  glEnable(GL_CULL_FACE);
}

// ---------------------------------------------------------------------------

void ImageUI::shutdown()
{
    // Delete all loaded textures
    for (unsigned int tex : textures) {
        if (tex) glDeleteTextures(1, &tex);
    }
    textures.clear();

    if (EBO) { glDeleteBuffers(1, &EBO);           EBO = 0; }
    if (VBO) { glDeleteBuffers(1, &VBO);           VBO = 0; }
    if (VAO) { glDeleteVertexArrays(1, &VAO);      VAO = 0; }
    if (shaderProgram) { glDeleteProgram(shaderProgram);     shaderProgram = 0; }
}


