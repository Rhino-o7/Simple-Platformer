#include <config.hpp>

#include <data/manager.hpp>
#include <data/text.hpp>
#include <data/data-shader.hpp>
#include <data/model.hpp>

#include <ecs/transform.hpp>
#include <ecs/behaviour.hpp>
#include <physics/collider.hpp>
#include <gl/camera.hpp>
#include <gl/light.hpp>
#include <gl/renderable.hpp>

#include <memory/string_stream_buffer.hpp>
#include <memory/text_stream.hpp>

#include <input/window.hpp>

#include <gl/renderer.hpp>
#include <gl/debug.hpp>

#include <ecs/flecs_runtime.hpp>
#include <game/runtime/scene_state.hpp>
#include <game/game_manager.hpp>

#include <glm/glm.hpp>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <sstream>
#include <random>

using namespace vpg;

bool load_game(game::runtime::SceneState* scene);

void APIENTRY gl_debug_output(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam) {

    // Ignore non-significant error/warning codes
    if (id == 131169 || id == 131185 || id == 131218 || id == 131204) {
        return;
    }

    std::cerr << "OpenGL debug message (" << id << "): " << message << std::endl;

    switch (source) {
    case GL_DEBUG_SOURCE_API:             std::cout << "Source: API" << std::endl; break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   std::cout << "Source: Window System" << std::endl; break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER: std::cout << "Source: Shader Compiler" << std::endl; break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:     std::cout << "Source: Third Party" << std::endl; break;
    case GL_DEBUG_SOURCE_APPLICATION:     std::cout << "Source: Application" << std::endl; break;
    case GL_DEBUG_SOURCE_OTHER:           std::cout << "Source: Other" << std::endl; break;
    }

    switch (type) {
    case GL_DEBUG_TYPE_ERROR:               std::cout << "Type: Error" << std::endl; break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: std::cout << "Type: Deprecated Behaviour" << std::endl; break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  std::cout << "Type: Undefined Behaviour"; break;
    case GL_DEBUG_TYPE_PORTABILITY:         std::cout << "Type: Portability" << std::endl; break;
    case GL_DEBUG_TYPE_PERFORMANCE:         std::cout << "Type: Performance" << std::endl; break;
    case GL_DEBUG_TYPE_MARKER:              std::cout << "Type: Marker" << std::endl; break;
    case GL_DEBUG_TYPE_PUSH_GROUP:          std::cout << "Type: Push Group" << std::endl; break;
    case GL_DEBUG_TYPE_POP_GROUP:           std::cout << "Type: Pop Group" << std::endl; break;
    case GL_DEBUG_TYPE_OTHER:               std::cout << "Type: Other" << std::endl; break;
    }

    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:         std::cout << "Severity: High" << std::endl; break;
    case GL_DEBUG_SEVERITY_MEDIUM:       std::cout << "Severity: Medium" << std::endl; break;
    case GL_DEBUG_SEVERITY_LOW:          std::cout << "Severity: Low" << std::endl; break;
    case GL_DEBUG_SEVERITY_NOTIFICATION: std::cout << "Severity: Notification" << std::endl; break;
    }

    std::cout << std::endl;
}
 int main(int argc, char** argv) {
    Config::load(argc, argv);

    if (!input::Window::init()) {
	std::cerr << "vpg::input::Window::init() failed:\n";
    	return 0;
    }

    // Initialize GLEW
    GLenum glew_status = glewInit();
    if (glew_status != GLEW_OK) {
        std::cerr << "glewInit() failed:\n"
                  << glewGetErrorString(glew_status) << '\n';
        return 1;
    }

    GLint flags;
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(gl_debug_output, nullptr);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
    }

    // Load assets
    data::Manager::register_type<data::Text>();
    data::Manager::register_type<data::Shader>();
    data::Manager::register_type<data::Model>();
    if (!data::Manager::init()) {
        std::cerr << "Couldn't initialize data manager\n";
        return 1;
    }

    game::runtime::SceneState scene;

    game::runtime::FlecsRuntime runtime;

    auto renderer = new gl::Renderer(&runtime.get_world());

    runtime.set_scene_loader([](const std::string& scene_name) {
        return Manager::load_scene(scene_name);
    });

    if (!load_game(&scene)) {
        std::cerr << "Couldn't load game\n";
        return 1;
    }

    // TODO: Fix delta time
    auto last_time = (float)glfwGetTime();
    auto update_dt = 1.0f / (float)Config::get_integer("update_fps", 60);
    auto lag = 0.0f;
    while (!input::Window::should_close()) {
        // Calculate delta time
        auto new_time = (float)glfwGetTime();
        auto delta_time = new_time - last_time;
        last_time = new_time;
        lag += delta_time;

        input::Window::poll_events();
        runtime.pump();

        while (lag >= update_dt) {
            runtime.run_fixed_update(update_dt);
            lag -= update_dt;
        }

        // Render here
        renderer->render(delta_time);

        input::Window::swap_buffers();

        // Calculate delta time

    }

    // Destroy renderer
    delete renderer;
    gl::Debug::terminate();

    scene.clean();

    // Unload assets
    data::Manager::terminate();

    input::Window::terminate();

    return 0;
}





