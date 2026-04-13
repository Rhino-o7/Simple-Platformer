#include "../include/corelib/corelib.hpp"

#include <config.hpp>

#include <data/manager.hpp>
#include <data/text.hpp>
#include <data/data-shader.hpp>
#include <data/model.hpp>

#include <input/window.hpp>

#include <gl/renderer.hpp>
#include <gl/debug.hpp>

#include <ecs/flecs_runtime.hpp>
#include <game/runtime/scene_state.hpp>
#include <game/game_manager.hpp>
#include <game/behaviour_registry.hpp>
#include <corelib/net/websocket.hpp>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include <filesystem>
#include <iostream>
#include <chrono>
#include <thread>
#include <memory>
#include <cstdlib>

using namespace vpg;
namespace fs = std::filesystem;

namespace {
    bool g_require_network_connection = false;

    void resolve_runtime_working_directory() {
        auto has_runtime_files = [](const fs::path& p) {
            return fs::exists(p / "vpg.cfg") && fs::exists(p / "data" / "assets.cfg");
        };

        fs::path p = fs::current_path();
        while (!p.empty()) {
            if (has_runtime_files(p)) {
                fs::current_path(p);
                return;
            }

            auto client_path = p / "Client";
            if (has_runtime_files(client_path)) {
                fs::current_path(client_path);
                return;
            }

            if (p == p.root_path()) {
                break;
            }

            p = p.parent_path();
        }
    }

    void APIENTRY gl_debug_output(
        GLenum source,
        GLenum type,
        GLuint id,
        GLenum severity,
        GLsizei length,
        const GLchar* message,
        const void* userParam) {

        (void)length;
        (void)userParam;

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
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  std::cout << "Type: Undefined Behaviour" << std::endl; break;
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
}

int corelib::run_client(int argc, char** argv) {
    resolve_runtime_working_directory();
    Config::load(argc, argv);

    if (!input::Window::init()) {
        std::cerr << "vpg::input::Window::init() failed:\n";
        return 0;
    }

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

    data::Manager::register_type<data::Text>();
    data::Manager::register_type<data::Shader>();
    data::Manager::register_type<data::Model>();
    if (!data::Manager::init()) {
        std::cerr << "Couldn't initialize data manager\n";
        return 1;
    }

    game::register_behaviours();

#ifdef __EMSCRIPTEN__
    struct EmLoopState {
        game::runtime::FlecsRuntime* runtime;
        gl::Renderer* renderer;
        game::runtime::SceneState* scene;
        float last_time;
        float update_dt;
        float lag;
    };

    auto* scene = new game::runtime::SceneState();
    Manager::scene = scene;

    auto* runtime = new game::runtime::FlecsRuntime();
    runtime->set_scene_loader([](const std::string& scene_name) {
        return Manager::load_scene(scene_name);
    });

    auto* renderer = new gl::Renderer(&runtime->get_world());

    if (!Manager::load()) {
        std::cerr << "Couldn't load game\n";
        delete renderer;
        delete runtime;
        delete scene;
        return 1;
    }

    auto* em_state = new EmLoopState{
        runtime,
        renderer,
        scene,
        (float)glfwGetTime(),
        1.0f / (float)Config::get_integer("update_fps", 60),
        0.0f
    };

    emscripten_set_main_loop_arg([](void* user_data) {
        auto* state = static_cast<EmLoopState*>(user_data);

        auto network = corelib::net::active_client();
        if (g_require_network_connection && (network == nullptr || !network->is_connection_alive())) {
            std::cerr << "Server connection lost. Closing client.\n";
            glfwSetWindowShouldClose((GLFWwindow*)input::Window::get_handle(), GLFW_TRUE);
        }

        auto new_time = (float)glfwGetTime();
        auto delta_time = new_time - state->last_time;
        state->last_time = new_time;
        state->lag += delta_time;

        input::Window::poll_events();
        state->runtime->pump();

        while (state->lag >= state->update_dt) {
            state->runtime->run_fixed_update(state->update_dt);
            state->lag -= state->update_dt;
        }

        state->renderer->render(delta_time);
        input::Window::swap_buffers();

        if (input::Window::should_close()) {
            delete state->renderer;
            delete state->runtime;
            gl::Debug::terminate();
            state->scene->clean();
            delete state->scene;
            data::Manager::terminate();
            input::Window::terminate();
            emscripten_cancel_main_loop();
            delete state;
        }
    }, em_state, 0, 1);

    return 0;
#else
    game::runtime::SceneState scene;
    Manager::scene = &scene;

    game::runtime::FlecsRuntime runtime;
    auto renderer = new gl::Renderer(&runtime.get_world());

    runtime.set_scene_loader([](const std::string& scene_name) {
        return Manager::load_scene(scene_name);
    });

    if (!Manager::load()) {
        std::cerr << "Couldn't load game\n";
        return 1;
    }

    auto last_time = (float)glfwGetTime();
    auto update_dt = 1.0f / (float)Config::get_integer("update_fps", 60);
    auto lag = 0.0f;
    while (!input::Window::should_close()) {
        auto network = corelib::net::active_client();
        if (g_require_network_connection && (network == nullptr || !network->is_connection_alive())) {
            std::cerr << "Server connection lost. Closing client.\n";
            glfwSetWindowShouldClose((GLFWwindow*)input::Window::get_handle(), GLFW_TRUE);
            break;
        }

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

        renderer->render(delta_time);
        input::Window::swap_buffers();
    }

    delete renderer;
    gl::Debug::terminate();

    scene.clean();
    data::Manager::terminate();
    input::Window::terminate();

    return 0;
#endif
}

int corelib::run_network_client(int argc, char** argv, const char* uri, bool reset_save) {
    std::string resolved_uri = (uri != nullptr) ? uri : "";

#ifdef __EMSCRIPTEN__
    auto prompt_text = [](const std::string& label, const std::string& fallback) {
        const char* raw = reinterpret_cast<const char*>(EM_ASM_PTR({
            const label = UTF8ToString($0);
            const fallback = UTF8ToString($1);
            const value = window.prompt(label, fallback);
            if (value === null) {
                return 0;
            }
            return stringToNewUTF8(value);
        }, label.c_str(), fallback.c_str()));

        if (raw == nullptr) {
            return fallback;
        }

        std::string out(raw);
        std::free(const_cast<char*>(raw));
        if (out.empty()) {
            return fallback;
        }

        return out;
    };

    std::string default_host = "127.0.0.1";
    std::string default_port = "9002";

    if (!resolved_uri.empty()) {
        const auto scheme_pos = resolved_uri.find("://");
        const auto host_start = (scheme_pos == std::string::npos) ? 0 : (scheme_pos + 3);
        const auto colon_pos = resolved_uri.rfind(':');

        if (colon_pos != std::string::npos && colon_pos > host_start) {
            default_host = resolved_uri.substr(host_start, colon_pos - host_start);
            default_port = resolved_uri.substr(colon_pos + 1);
        }
    }

    const std::string host = prompt_text("Server host [127.0.0.1]:", default_host);
    const std::string port = prompt_text("Server port [9002]:", default_port);
    resolved_uri = "ws://" + host + ":" + port;
#endif

    if (resolved_uri.empty()) {
        std::cerr << "Network client requires a server URI\n";
        return 1;
    }

    constexpr auto retry_interval = std::chrono::seconds(2);
    constexpr auto initial_retry_window = std::chrono::seconds(30);
    constexpr auto reconnect_retry_window = std::chrono::seconds(30);

    std::unique_ptr<corelib::net::GameClient> client;
    g_require_network_connection = true;
    corelib::net::set_active_client(nullptr);

    bool reset_save_pending = reset_save;

    auto try_connect_for = [&](std::chrono::seconds max_duration) {
        const auto start = std::chrono::steady_clock::now();
        int attempt = 1;

        while (true) {
            client = std::make_unique<corelib::net::GameClient>();
            corelib::net::set_active_client(client.get());
            std::cout << "[CLIENT] connect attempt " << attempt << " to " << resolved_uri << "\n";
            if (client->connect(resolved_uri)) {
#ifdef __EMSCRIPTEN__
                if (reset_save_pending) {
                    client->send_event("RESET_SAVE", 0);
                    reset_save_pending = false;
                }
                return true;
#else
                const auto open_wait_start = std::chrono::steady_clock::now();
                while (!client->connected()) {
                    if (std::chrono::steady_clock::now() - open_wait_start >= std::chrono::seconds(5)) {
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }

                const auto asset_wait_start = std::chrono::steady_clock::now();
                while (client->connected() && !client->has_server_level_layout()) {
                    if (std::chrono::steady_clock::now() - asset_wait_start >= std::chrono::seconds(5)) {
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }

                if (!client->connected() || !client->has_server_level_layout()) {
                    std::cerr << "[CLIENT] connected but server assets were not received in time\n";
                    client->stop();
                    client.reset();
                    corelib::net::set_active_client(nullptr);
                }
                else {
                if (reset_save_pending) {
                    client->send_event("RESET_SAVE", 0);
                    reset_save_pending = false;
                }
                return true;
                }
#endif
            }

            if (client != nullptr) {
                client->stop();
                client.reset();
                corelib::net::set_active_client(nullptr);
            }

            const auto now = std::chrono::steady_clock::now();
            if (now - start >= max_duration) {
                return false;
            }

            std::this_thread::sleep_for(retry_interval);
            ++attempt;
        }
    };

    if (!try_connect_for(initial_retry_window)) {
        g_require_network_connection = false;
        corelib::net::set_active_client(nullptr);
        std::cerr << "Failed to connect to server within " << initial_retry_window.count()
                  << " seconds: " << resolved_uri << '\n';
        return 1;
    }

#ifdef __EMSCRIPTEN__
    return run_client(argc, argv);
#else
    while (true) {
        const int result = run_client(argc, argv);
        const bool disconnected = (client == nullptr) || !client->is_connection_alive();
        if (client != nullptr) {
            client->stop();
            client.reset();
        }
        corelib::net::set_active_client(nullptr);

        if (!disconnected) {
            g_require_network_connection = false;
            return result;
        }

        std::cout << "[CLIENT] server connection lost, attempting reconnect...\n";
        if (!try_connect_for(reconnect_retry_window)) {
            g_require_network_connection = false;
            corelib::net::set_active_client(nullptr);
            std::cerr << "Reconnect timed out after " << reconnect_retry_window.count() << " seconds.\n";
            return 1;
        }
    }
#endif
}
