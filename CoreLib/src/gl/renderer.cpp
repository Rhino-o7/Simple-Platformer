#include <gl/renderer.hpp>
#include <gl/debug.hpp>
#include <config.hpp>
#include <ecs/transform.hpp>
#include "text_ui.hpp"
#include "shader.hpp"
#include "camera.hpp"
#include <gl/light.hpp>
#include <gl/renderable.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <GL/glew.h>
#include <random>
#include <iostream>
#include <map>
#include <string>

#define LIGHT_COUNT 64
#include <ft2build.h>
#include <freetype/ftglyph.h>
#include <freetype/ftoutln.h>
#include <freetype/fttrigon.h>
#include FT_FREETYPE_H  

unsigned int VAO, VBO;

using namespace vpg;
using namespace vpg::gl;

vpg::gl::Renderer::Renderer(flecs::world* ecs_world) {
#pragma region not freetype
    this->size = input::Window::get_framebuffer_size();
    this->ecs_world = ecs_world;
    this->camera_query = this->ecs_world->query<vpg::ecs::Transform, const vpg::gl::Camera>();
    this->light_query = this->ecs_world->query<vpg::ecs::Transform, const vpg::gl::Light>();
    this->renderable_query = this->ecs_world->query<vpg::ecs::Transform, const vpg::gl::Renderable>();

    
    this->gbuffer.shader = data::Manager::load<data::Shader>("shader.gbuffer");
    this->gbuffer.shader->get_shader().bind_uniform_buffer("Lights", 0);
    this->ssao.shader = data::Manager::load<data::Shader>("shader.ssao");
    this->ssao_blur.shader = data::Manager::load<data::Shader>("shader.ssao_blur");
    this->model_shader = data::Manager::load<data::Shader>("shader.model");
    this->model_shader->get_shader().bind_uniform_buffer("Palette", 0);

    this->wireframe = false;
    this->debug_lights = false;
    this->debug_rendering = false;

    glGenVertexArrays(1, &this->screen_va);

    glGenBuffers(1, &this->lights_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, this->lights_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(LightData) * LIGHT_COUNT, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    this->create_gbuffer();
    this->create_ssao();

    // Optional: init text UI (use your data folder + font path)
    std::string font_path = vpg::Config::get_string("data.folder", "./data/") + "fonts/arial.ttf";
    if (!this->text_ui.init(font_path.c_str(), 32))
    {
        this->text_ui.init("C:/Windows/Fonts/arial.ttf", 32);  // fallback on Windows
    }
    
    int width, height, nrChannels;
    unsigned char* dataa = stbi_load("data/images/red.jpg", &width, &height, &nrChannels, 0);
    unsigned char* dataaa = stbi_load("data/images/black.jpg", &width, &height, &nrChannels, 0);
    this->image_ui.init();
    this->health_tex_id = this->image_ui.load_texture(width, height, dataa);
    this->black_id = this->image_ui.load_texture(width, height, dataaa);
    stbi_image_free(dataa);

    this->resize_listener = input::Window::FramebufferResized.add_listener(
        std::bind(&Renderer::resize_callback, this, std::placeholders::_1)
    );
    this->debug_render_toggle_listener = input::Keyboard::Down.add_listener(
        std::bind(&Renderer::debug_render_toggle_callback, this, std::placeholders::_1)
    );
#pragma endregion
}

vpg::gl::Renderer::~Renderer() {
    input::Window::FramebufferResized.remove_listener(this->resize_listener);
    input::Keyboard::Down.remove_listener(this->debug_render_toggle_listener);

    this->destroy_ssao();
    this->destroy_gbuffer();
    this->text_ui.shutdown();
    this->image_ui.shutdown();

    glDeleteBuffers(1, &this->lights_ubo);
    glDeleteVertexArrays(1, &this->screen_va);
}

void vpg::gl::Renderer::render(float dt) {
    // Get camera
#pragma region Rendering
    const vpg::gl::Camera* camera_snapshot = nullptr;
    vpg::ecs::Transform* camera_transform = nullptr;
    auto found_camera = false;
    camera_query.each(
        [&found_camera, &camera_snapshot, &camera_transform](vpg::ecs::Transform& t, const vpg::gl::Camera& c) {
            if (found_camera) {
                return;
            }

            camera_snapshot = &c;
            camera_transform = &t;
            found_camera = true;
        }
    );

    if (!found_camera) {
        return;
    }

    auto camera_proj = glm::perspective(
        glm::radians(camera_snapshot->get_fov()),
        (float)this->size.x / (float)this->size.y,
        camera_snapshot->get_z_near(),
        camera_snapshot->get_z_far()
    );
    auto camera_view = glm::inverse(camera_transform->get_global());
    this->par_x = camera_transform->get_global_position().x;
    this->par_y = camera_transform->get_global_position().y;
    this->par_z = camera_transform->get_global_position().z;
    //RenderText("Hello World", camera.cam_x, camera.cam_y, camera.cam_z, 2.0f, glm::vec3(1, 0, 1));

    // Update lights UBO
    std::vector<LightData> lights_cpu(LIGHT_COUNT);
    LightData* lights = lights_cpu.data();
    int light_index = 0;
    auto draw_light = [&](const vpg::gl::Light& light, vpg::ecs::Transform& transform) {

        switch (light.type) {
        case Light::Type::Directional:
            lights[light_index].ambient = glm::vec4(light.ambient, 1.0f);
            lights[light_index].diffuse = glm::vec4(light.diffuse, 1.0f);
            lights[light_index].direction = camera_view * glm::vec4(transform.get_global_rotation() * glm::vec3(0.0f, 0.0f, 1.0f), 0.0f);
            break;
        case Light::Type::Point:
            if (this->debug_lights) {
                gl::Debug::draw_sphere(transform.get_global_position(), 1.0f, lights[light_index].diffuse);
            }
            lights[light_index].position = camera_view * glm::vec4(transform.get_global_position(), 1.0f);
            lights[light_index].direction.w = 1.0f;
            lights[light_index].constant = light.constant;
            lights[light_index].linear = light.linear;
            lights[light_index].quadratic = light.quadratic;
            lights[light_index].ambient = glm::vec4(light.ambient, 1.0f);
            lights[light_index].diffuse = glm::vec4(light.diffuse, 1.0f);
            break;
        }

        light_index += 1;
    };

    light_query.each(
        [&](vpg::ecs::Transform& transform, const vpg::gl::Light& light) {
            if (light_index >= LIGHT_COUNT) {
                return;
            }

            draw_light(light, transform);
        }
    );
    for (; light_index < LIGHT_COUNT; ++light_index) {
        lights[light_index].ambient = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
        lights[light_index].diffuse = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    }
    glBindBuffer(GL_UNIFORM_BUFFER, this->lights_ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(LightData) * LIGHT_COUNT, lights_cpu.data());
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glViewport(0, 0, this->size.x, this->size.y);

#ifdef __EMSCRIPTEN__
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glPolygonMode(GL_FRONT_AND_BACK, this->wireframe ? GL_LINE : GL_FILL);
    glDisable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    glClearColor(0.60f, 0.62f, 0.78f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    auto model_loc_web = this->model_shader->get_shader().get_uniform_location("model");
    auto view_loc_web = this->model_shader->get_shader().get_uniform_location("view");
    auto proj_loc_web = this->model_shader->get_shader().get_uniform_location("proj");
    this->model_shader->get_shader().bind();
    glUniformMatrix4fv(view_loc_web, 1, GL_FALSE, &camera_view[0][0]);
    glUniformMatrix4fv(proj_loc_web, 1, GL_FALSE, &camera_proj[0][0]);

    int submitted_models = 0;
    renderable_query.each(
        [&](vpg::ecs::Transform& transform, const vpg::gl::Renderable& renderable) {
            if (renderable.type != Renderable::Type::Model || renderable.model.get_asset() == nullptr) {
                return;
            }

            submitted_models += 1;
            auto model_matrix = transform.get_global();
            glUniformMatrix4fv(model_loc_web, 1, GL_FALSE, &model_matrix[0][0]);
            renderable.model->get_palette().bind(0);
            renderable.model->get_vertex_array().bind();
            renderable.model->get_index_buffer().bind();
            glDrawElements(GL_TRIANGLES, renderable.model->get_index_count(), GL_UNSIGNED_INT, nullptr);
        }
    );

    static int web_log_counter = 0;
    if ((web_log_counter++ % 180) == 0) {
        std::cout << "[WEB] submitted_models=" << submitted_models
                  << " cam=(" << camera_transform->get_global_position().x << ","
                  << camera_transform->get_global_position().y << ","
                  << camera_transform->get_global_position().z << ")\n";
    }

    this->text_ui.render("Health: " + std::to_string(camera_snapshot->player_health), 10.0f, (float)this->size.y - 50.0f, .75,
        glm::vec3(1.0f, 1.0f, 0.0f), this->size.x, this->size.y);
    this->text_ui.render("Wind Speed: " + std::to_string(camera_snapshot->player_wind), 10.0f, (float)this->size.y - 80.0f, .75,
        glm::vec3(1.0f, 1.0f, 0.0f), this->size.x, this->size.y);
    this->text_ui.render("Time Left: " + std::to_string(camera_snapshot->seconds), 10.0f, (float)this->size.y - 110.0f, .75,
        glm::vec3(1.0f, 1.0f, 0.0f), this->size.x, this->size.y);
    this->text_ui.render("Level " + std::to_string(camera_snapshot->level), 10.0f, (float)this->size.y - 140.0f, .75,
        glm::vec3(1.0f, 1.0f, 0.0f), this->size.x, this->size.y);

    return;
#endif

    // Opaque pass
    glBindFramebuffer(GL_FRAMEBUFFER, this->gbuffer.fbo);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glPolygonMode(GL_FRONT_AND_BACK, this->wireframe ? GL_LINE : GL_FILL);
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);

    // Clear framebuffer
    GLuint draw_buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, &draw_buffers[0]);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    auto model_loc = this->model_shader->get_shader().get_uniform_location("model");
    auto view_loc = this->model_shader->get_shader().get_uniform_location("view");
    auto proj_loc = this->model_shader->get_shader().get_uniform_location("proj");
    this->model_shader->get_shader().bind();

    glUniformMatrix4fv(view_loc, 1, GL_FALSE, &camera_view[0][0]);
    glUniformMatrix4fv(proj_loc, 1, GL_FALSE, &camera_proj[0][0]);

    auto draw_renderable = [&](const vpg::gl::Renderable& renderable,
        vpg::ecs::Transform& transform) {
        auto model_matrix = transform.get_global();
        glUniformMatrix4fv(model_loc, 1, GL_FALSE, &model_matrix[0][0]);

        switch (renderable.type) {
        case Renderable::Type::Model:
            if (renderable.model.get_asset() != nullptr) {
                renderable.model->get_palette().bind(0);
                renderable.model->get_vertex_array().bind();
                renderable.model->get_index_buffer().bind();
                glDrawElements(GL_TRIANGLES, renderable.model->get_index_count(), GL_UNSIGNED_INT, nullptr);
            }
            break;
        }
    };

    renderable_query.each(
        [&](vpg::ecs::Transform& transform, const vpg::gl::Renderable& renderable) {
            draw_renderable(renderable, transform);
        }
    );

    // SSAO pass
    glBindVertexArray(this->screen_va);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glBindFramebuffer(GL_FRAMEBUFFER, this->ssao.fbo);
    glDrawBuffers(1, &draw_buffers[0]);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, this->gbuffer.position);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, this->gbuffer.normal);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, this->ssao.noise);

    auto noise_scale = glm::vec2(float(this->size.x) / 4.0f, float(this->size.y) / 4.0f);

    auto& ssao_shader = this->ssao.shader->get_shader();
    ssao_shader.bind();
    glUniform1i(ssao_shader.get_uniform_location("position_tex"), 0);
    glUniform1i(ssao_shader.get_uniform_location("normal_tex"), 1);
    glUniform1i(ssao_shader.get_uniform_location("noise_tex"), 2);
    glUniform3fv(ssao_shader.get_uniform_location("samples"), this->ssao.samples.size(), &this->ssao.samples[0][0]);
    glUniform2fv(ssao_shader.get_uniform_location("noise_scale"), 1, &noise_scale[0]);
    glUniformMatrix4fv(ssao_shader.get_uniform_location("projection"), 1, GL_FALSE, &camera_proj[0][0]);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // SSAO Blur pass
    glBindFramebuffer(GL_FRAMEBUFFER, this->ssao_blur.fbo);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, this->ssao.color_buffer);

    auto& ssao_blur_shader = this->ssao_blur.shader->get_shader();
    ssao_blur_shader.bind();
    glUniform1i(ssao_blur_shader.get_uniform_location("ssao_tex"), 0);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // GBuffer pass
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, this->gbuffer.albedo);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, this->gbuffer.position);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, this->gbuffer.normal);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, this->ssao_blur.color_buffer);

    auto& lighting_shader = this->gbuffer.shader->get_shader();
    lighting_shader.bind();
    glUniform1i(lighting_shader.get_uniform_location("albedo_tex"), 0);
    glUniform1i(lighting_shader.get_uniform_location("position_tex"), 1);
    glUniform1i(lighting_shader.get_uniform_location("normal_tex"), 2);
    glUniform1i(lighting_shader.get_uniform_location("ssao_tex"), 3);
    glUniformMatrix4fv(lighting_shader.get_uniform_location("proj"), 1, GL_FALSE, &camera_proj[0][0]);
    glUniformMatrix4fv(lighting_shader.get_uniform_location("view"), 1, GL_FALSE, &camera_view[0][0]);
    glUniform1f(lighting_shader.get_uniform_location("z_far"), camera_snapshot->get_z_far());
    glBindBufferRange(GL_UNIFORM_BUFFER, 0, this->lights_ubo, 0, sizeof(Light) * LIGHT_COUNT);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Debug draw on top of screen
    glBindFramebuffer(GL_READ_FRAMEBUFFER, this->gbuffer.fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, this->size.x, this->size.y, 0, 0, this->size.x, this->size.y, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    if (this->debug_rendering) {
        auto vp = camera_proj * camera_view;
        Debug::flush(vp, dt);
    }
#pragma endregion
    // Text UI on top of first scene
    this->text_ui.render("Health: " + std::to_string(camera_snapshot->player_health), 10.0f, (float)this->size.y - 50.0f, .75,
        glm::vec3(1.0f, 1.0f, 0.0f), this->size.x, this->size.y);
    this->text_ui.render("Wind Speed: " + std::to_string(camera_snapshot->player_wind), 10.0f, (float)this->size.y - 80.0f, .75,
        glm::vec3(1.0f, 1.0f, 0.0f), this->size.x, this->size.y);
    this->text_ui.render("Time Left: " + std::to_string(camera_snapshot->seconds), 10.0f, (float)this->size.y - 110.0f, .75,
        glm::vec3(1.0f, 1.0f, 0.0f), this->size.x, this->size.y);
    this->text_ui.render("Level " + std::to_string(camera_snapshot->level), 10.0f, (float)this->size.y - 140.0f, .75,
        glm::vec3(1.0f, 1.0f, 0.0f), this->size.x, this->size.y);

#ifndef __EMSCRIPTEN__
    this->image_ui.DrawImage(this->black_id, 1,
        -1, 1, 0.5);
    this->image_ui.DrawImage(this->health_tex_id, camera_snapshot->player_health / static_cast<float>(3),
        -1, 1, 0.5);
#endif
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void vpg::gl::Renderer::resize_callback(glm::ivec2 size) {
    this->destroy_gbuffer();
    this->destroy_ssao();
    this->size = size;
    this->create_gbuffer();
    this->create_ssao();
}

void vpg::gl::Renderer::debug_render_toggle_callback(input::Keyboard::Key key) {
    using Key = input::Keyboard::Key;
    switch (key) {
    case Key::F1:
        this->wireframe = !this->wireframe;
        break;
    case Key::F2:
        this->debug_rendering = !this->debug_rendering;
        if (this->debug_rendering) {
            Debug::init();
        }
        else {
            Debug::terminate();
        }
        break;
    case Key::F3:
        this->debug_lights = !this->debug_lights;
        break;
    }
}

void vpg::gl::Renderer::create_gbuffer() {
    // Prepare GBuffer framebuffer and textures
    glGenFramebuffers(1, &this->gbuffer.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, this->gbuffer.fbo);

    glGenTextures(1, &this->gbuffer.albedo);
    glBindTexture(GL_TEXTURE_2D, this->gbuffer.albedo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->size.x, this->size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->gbuffer.albedo, 0);

    glGenTextures(1, &this->gbuffer.position);
    glBindTexture(GL_TEXTURE_2D, this->gbuffer.position);
#ifdef __EMSCRIPTEN__
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, this->size.x, this->size.y, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
#else
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, this->size.x, this->size.y, 0, GL_RGB, GL_FLOAT, nullptr);
#endif
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, this->gbuffer.position, 0);

    glGenTextures(1, &this->gbuffer.normal);
    glBindTexture(GL_TEXTURE_2D, this->gbuffer.normal);
#ifdef __EMSCRIPTEN__
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, this->size.x, this->size.y, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
#else
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, this->size.x, this->size.y, 0, GL_RGB, GL_FLOAT, nullptr);
#endif
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, this->gbuffer.normal, 0);

    glGenTextures(1, &this->gbuffer.depth);
    glBindTexture(GL_TEXTURE_2D, this->gbuffer.depth);
#ifdef __EMSCRIPTEN__
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, this->size.x, this->size.y, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
#else
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, this->size.x, this->size.y, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, nullptr);
#endif
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, this->gbuffer.depth, 0);

    const GLenum gbuffer_attachments[3] = {
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2
    };
    glDrawBuffers(3, gbuffer_attachments);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "vpg::gl::Renderer::create_gbuffer() failed:\n"
                  << "GBuffer framebuffer is not complete\n";
        abort();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void vpg::gl::Renderer::destroy_gbuffer() {
    glDeleteFramebuffers(1, &this->gbuffer.fbo);
    glDeleteTextures(1, &this->gbuffer.albedo);
    glDeleteTextures(1, &this->gbuffer.position);
    glDeleteTextures(1, &this->gbuffer.normal);
    glDeleteTextures(1, &this->gbuffer.depth);
}

void vpg::gl::Renderer::create_ssao() {
    // SSAO framebuffer and textures
    glGenFramebuffers(1, &this->ssao.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, this->ssao.fbo);

    glGenTextures(1, &this->ssao.color_buffer);
    glBindTexture(GL_TEXTURE_2D, this->ssao.color_buffer);
#ifdef __EMSCRIPTEN__
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, this->size.x, this->size.y, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
#else
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, this->size.x, this->size.y, 0, GL_RED, GL_FLOAT, NULL);
#endif
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->ssao.color_buffer, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "vpg::gl::Renderer::create_ssao() failed:\n"
                  << "SSAO framebuffer is not complete\n";
        abort();
    }

    // SSAO kernel
    std::uniform_real_distribution<float> random_floats(0.0f, 1.0f);
    std::default_random_engine generator;
    this->ssao.samples.resize(64);
    for (int i = 0; i < this->ssao.samples.size(); ++i) {
        auto sample = glm::vec3(
            random_floats(generator) * 2.0 - 1.0,
            random_floats(generator) * 2.0 - 1.0,
            random_floats(generator)
        );
        sample = glm::normalize(sample) * random_floats(generator);

        float scale = (float)i / (float)this->ssao.samples.size();
        scale = 0.1f + scale * scale * (1.0f - 0.1f);
        sample *= scale;

        this->ssao.samples[i] = sample;
    }

    // SSAO noise texture
    std::vector<glm::vec4> ssao_noise;
    for (int i = 0; i < 16; ++i) {
        ssao_noise.push_back({
            random_floats(generator) * 2.0 - 1.0,
            random_floats(generator) * 2.0 - 1.0,
            0.0f,
            0.0f
            });
    }

    glGenTextures(1, &this->ssao.noise);
    glBindTexture(GL_TEXTURE_2D, this->ssao.noise);
#ifdef __EMSCRIPTEN__
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 4, 4, 0, GL_RGBA, GL_HALF_FLOAT, &ssao_noise[0]);
#else
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 4, 4, 0, GL_RGBA, GL_FLOAT, &ssao_noise[0]);
#endif
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // SSAO blur framebuffer and texture
    glGenFramebuffers(1, &this->ssao_blur.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, this->ssao_blur.fbo);

    glGenTextures(1, &this->ssao_blur.color_buffer);
    glBindTexture(GL_TEXTURE_2D, this->ssao_blur.color_buffer);
#ifdef __EMSCRIPTEN__
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, this->size.x, this->size.y, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
#else
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, this->size.x, this->size.y, 0, GL_RED, GL_FLOAT, NULL);
#endif
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->ssao_blur.color_buffer, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "vpg::gl::Renderer::create_ssao() failed:\n"
                  << "SSAO blur framebuffer is not complete\n";
        abort();
    }
}

void vpg::gl::Renderer::destroy_ssao() {
    glDeleteFramebuffers(1, &this->ssao.fbo);
    glDeleteTextures(1, &this->ssao.color_buffer);
    glDeleteTextures(1, &this->ssao.noise);
    glDeleteFramebuffers(1, &this->ssao_blur.fbo);
    glDeleteTextures(1, &this->ssao_blur.color_buffer);
}





