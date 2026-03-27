#pragma once

#include <data/data-shader.hpp>
#include <ecs/entity_manager.hpp>

#include <input/window.hpp>
#include <input/keyboard.hpp>
#include <gl/text_ui.hpp>

#ifndef flecs_STATIC
#define flecs_STATIC
#endif
#include <flecs.h>

#include <string>
#include "stb_image.h"
#include "image_ui.hpp"

namespace vpg::gl {
    class Renderer {
    public:
        int player_health;
        Renderer(flecs::world* ecs_world);
        void RenderText(std::string text, float x, float y, float z, float scale, glm::vec3 color);
        ~Renderer();

        void render(float dt);

    private:
        void create_gbuffer();
        void destroy_gbuffer();
        void create_ssao();
        void destroy_ssao();

        void resize_callback(glm::ivec2 size);
        Listener resize_listener;

        void debug_render_toggle_callback(input::Keyboard::Key key);
        Listener debug_render_toggle_listener;

        flecs::world* ecs_world;

        data::Handle<data::Shader> model_shader;
        gl::TextUI text_ui;
        gl::ImageUI image_ui;
        int width, height, nrChannels, image_id;
        unsigned char* data;// = stbi_load("red_gradient.jpg", &width, &height, &nrChannels, 0);
        int health_tex_id, black_id;
        ecs::Entity player;

        float par_x;
        float par_y;
        float par_z;
        //Camera cameraPar;

        bool wireframe;
        bool debug_lights;
        bool debug_rendering;

        glm::ivec2 size;
        unsigned int screen_va;

        struct LightData {
            glm::vec4 position = { 0.0f, 0.0f, 0.0f, 1.0f };
            glm::vec4 direction = { 0.0f, 0.0f, 0.0f, 0.0f };
            glm::vec4 ambient = { 0.0f, 0.0f, 0.0f, 1.0f };
            glm::vec4 diffuse = { 0.0f, 0.0f, 0.0f, 1.0f };
            float constant = 0.0f;
            float linear = 0.0f;
            float quadratic = 0.0f;
            float _padding = 0.0f;
        };
        unsigned int lights_ubo;

        struct {
            unsigned int fbo;
            unsigned int albedo, position, normal, depth;
            data::Handle<data::Shader> shader;
        } gbuffer;

        struct {
            unsigned int fbo;
            unsigned int color_buffer;
            unsigned int noise;
            std::vector<glm::vec3> samples;
            data::Handle<data::Shader> shader;
        } ssao;

        struct {
            unsigned int fbo;
            unsigned int color_buffer;
            data::Handle<data::Shader> shader;
        } ssao_blur;
    };
};

