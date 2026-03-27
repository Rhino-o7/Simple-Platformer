#pragma once

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <memory>

struct FT_LibraryRec_;
struct FT_FaceRec_;

namespace vpg::gl {

class TextUI {
public:
    TextUI() = default;
    ~TextUI();

    bool init(const char* font_path, int pixel_height = 48);
    void shutdown();

    void render(const std::string& text, float x, float y, float scale,
                glm::vec3 color, int viewport_width, int viewport_height);

private:
    struct Glyph {
        unsigned int texture_id;
        int width, height;
        int bearing_x, bearing_y;
        long advance;
    };

    FT_LibraryRec_* ft = nullptr;
    FT_FaceRec_* face = nullptr;
    std::unordered_map<char, Glyph> glyphs;

    unsigned int shader_prog = 0;
    unsigned int vao = 0, vbo = 0;

    void load_glyph(char c);
    void use_ortho(int w, int h);
};
}
