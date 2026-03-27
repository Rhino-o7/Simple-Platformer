#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace vpg::gl {

    class ImageUI {
    public:
        ImageUI() = default;
        ~ImageUI();
        void shutdown();

        bool init();
        int load_texture(int width, int height, unsigned char* data);
        void DrawImage(int texture_id, float crop_percent, float x1, float y1, float w1);

    private:
        unsigned int shaderProgram = 0;
        unsigned int VAO = 0, VBO = 0, EBO = 0;

        std::vector<unsigned int> textures;
    };

} // namespace vpg::gl