#include <corelib/corelib.hpp>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/websocket.h>

#include <GLFW/glfw3.h>
#include <GLES3/gl3.h>
#endif

#include <chrono>
#include <iostream>
#include <sstream>
#include <string>

namespace {
#ifdef __EMSCRIPTEN__
    struct PlayerState {
        int id = 0;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float wind = 0.0f;
        int health = 3;
        int seconds = 120;
        int level = 1;
        int respawn_revision = 0;
    };

    bool parse_state_message(const std::string& payload, PlayerState& out_state) {
        std::istringstream in(payload);
        std::string type;
        in >> type;
        if (type != "STATE") {
            return false;
        }

        in >> out_state.id >> out_state.x >> out_state.y >> out_state.z;
        if (in.fail()) {
            return false;
        }

        if (!(in >> out_state.wind >> out_state.health >> out_state.seconds)) {
            out_state.wind = 0.0f;
            out_state.health = 3;
            out_state.seconds = 120;
            out_state.level = 1;
            out_state.respawn_revision = 0;
            return true;
        }

        if (!(in >> out_state.level >> out_state.respawn_revision)) {
            out_state.level = 1;
            out_state.respawn_revision = 0;
        }

        return !in.fail();
    }

    EMSCRIPTEN_WEBSOCKET_T g_socket = 0;
    bool g_connected = false;
    bool g_reset_save = false;
    std::string g_uri;

    PlayerState g_latest_state{};
    bool g_has_state = false;

    GLFWwindow* g_window = nullptr;
    GLuint g_program = 0;
    GLuint g_vbo = 0;
    GLuint g_vao = 0;

    std::chrono::steady_clock::time_point g_last_input_send{};
    float g_last_move_x = 0.0f;
    float g_last_move_z = 0.0f;
    bool g_last_jump = false;
    bool g_last_sprint = false;

    bool create_shader_program() {
        const char* vs = R"(
            #version 300 es
            precision highp float;
            layout(location = 0) in vec2 aPos;
            void main() {
                gl_Position = vec4(aPos, 0.0, 1.0);
            }
        )";

        const char* fs = R"(
            #version 300 es
            precision highp float;
            out vec4 FragColor;
            void main() {
                FragColor = vec4(0.25, 0.9, 0.35, 1.0);
            }
        )";

        auto compile_shader = [](GLenum type, const char* src) -> GLuint {
            GLuint shader = glCreateShader(type);
            glShaderSource(shader, 1, &src, nullptr);
            glCompileShader(shader);

            GLint ok = 0;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
            if (!ok) {
                char log[512]{};
                glGetShaderInfoLog(shader, 511, nullptr, log);
                std::cerr << "Shader compile failed: " << log << std::endl;
                glDeleteShader(shader);
                return 0;
            }
            return shader;
        };

        GLuint vert = compile_shader(GL_VERTEX_SHADER, vs);
        GLuint frag = compile_shader(GL_FRAGMENT_SHADER, fs);
        if (!vert || !frag) {
            return false;
        }

        g_program = glCreateProgram();
        glAttachShader(g_program, vert);
        glAttachShader(g_program, frag);
        glLinkProgram(g_program);

        glDeleteShader(vert);
        glDeleteShader(frag);

        GLint linked = 0;
        glGetProgramiv(g_program, GL_LINK_STATUS, &linked);
        if (!linked) {
            char log[512]{};
            glGetProgramInfoLog(g_program, 511, nullptr, log);
            std::cerr << "Program link failed: " << log << std::endl;
            glDeleteProgram(g_program);
            g_program = 0;
            return false;
        }

        glGenVertexArrays(1, &g_vao);
        glGenBuffers(1, &g_vbo);
        glBindVertexArray(g_vao);
        glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 12, nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, nullptr);

        return true;
    }

    void send_text(const std::string& message) {
        if (!g_connected || g_socket <= 0) {
            return;
        }

        emscripten_websocket_send_utf8_text(g_socket, message.c_str());
    }

    EM_BOOL on_open(int, const EmscriptenWebSocketOpenEvent*, void*) {
        g_connected = true;
        std::cout << "[WEB CLIENT] connected" << std::endl;

        send_text("HELLO web_client");
        send_text("SPAWN 0 15 0");

        if (g_reset_save) {
            send_text("EVENT RESET_SAVE 0");
        }

        return EM_TRUE;
    }

    EM_BOOL on_close(int, const EmscriptenWebSocketCloseEvent* e, void*) {
        g_connected = false;
        std::cout << "[WEB CLIENT] disconnected code=" << e->code << std::endl;
        return EM_TRUE;
    }

    EM_BOOL on_error(int, const EmscriptenWebSocketErrorEvent*, void*) {
        g_connected = false;
        std::cout << "[WEB CLIENT] websocket error" << std::endl;
        return EM_TRUE;
    }

    EM_BOOL on_message(int, const EmscriptenWebSocketMessageEvent* e, void*) {
        if (!e->isText || e->numBytes == 0 || e->data == nullptr) {
            return EM_TRUE;
        }

        std::string msg(reinterpret_cast<const char*>(e->data), static_cast<size_t>(e->numBytes));
        if (msg.rfind("STATE", 0) == 0) {
            PlayerState state{};
            if (parse_state_message(msg, state)) {
                g_latest_state = state;
                g_has_state = true;
            }
        }
        else {
            std::cout << "[SERVER -> WEB CLIENT] " << msg << std::endl;
        }

        return EM_TRUE;
    }

    void send_input_from_keyboard() {
        if (!g_window) {
            return;
        }

        float move_x = 0.0f;
        float move_z = 0.0f;

        if (glfwGetKey(g_window, GLFW_KEY_A) == GLFW_PRESS) move_x -= 1.0f;
        if (glfwGetKey(g_window, GLFW_KEY_D) == GLFW_PRESS) move_x += 1.0f;
        if (glfwGetKey(g_window, GLFW_KEY_W) == GLFW_PRESS) move_z += 1.0f;
        if (glfwGetKey(g_window, GLFW_KEY_S) == GLFW_PRESS) move_z -= 1.0f;

        const bool jump = glfwGetKey(g_window, GLFW_KEY_SPACE) == GLFW_PRESS;
        const bool sprint = glfwGetKey(g_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
            || glfwGetKey(g_window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;

        const auto now = std::chrono::steady_clock::now();
        const bool changed = move_x != g_last_move_x
            || move_z != g_last_move_z
            || jump != g_last_jump
            || sprint != g_last_sprint;

        if (!changed && (now - g_last_input_send) < std::chrono::milliseconds(50)) {
            return;
        }

        std::ostringstream out;
        out << "INPUT " << move_x << ' ' << move_z << ' ' << (jump ? 1 : 0) << ' ' << (sprint ? 1 : 0);
        send_text(out.str());

        g_last_input_send = now;
        g_last_move_x = move_x;
        g_last_move_z = move_z;
        g_last_jump = jump;
        g_last_sprint = sprint;
    }

    void render_state() {
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(g_window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.06f, 0.06f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (!g_has_state || !g_program) {
            glfwSwapBuffers(g_window);
            return;
        }

        const float scale = 1.0f / 30.0f;
        const float px = g_latest_state.x * scale;
        const float py = g_latest_state.z * scale;
        const float half = 0.035f;

        const float verts[] = {
            px - half, py - half,
            px + half, py - half,
            px + half, py + half,
            px - half, py - half,
            px + half, py + half,
            px - half, py + half
        };

        glUseProgram(g_program);
        glBindVertexArray(g_vao);
        glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glfwSwapBuffers(g_window);
    }

    void idle_tick() {
        if (!g_window) {
            return;
        }

        glfwPollEvents();
        send_input_from_keyboard();
        render_state();

        if (glfwWindowShouldClose(g_window)) {
            emscripten_cancel_main_loop();
        }
    }
#endif
}

int corelib::run_client(int, char**) {
    return 0;
}

int corelib::run_network_client(int, char**, const char* uri, bool reset_save) {
#ifndef __EMSCRIPTEN__
    (void)uri;
    (void)reset_save;
    std::cerr << "Client-Web network runtime is only available when built with Emscripten." << std::endl;
    return 1;
#else
    if (!emscripten_websocket_is_supported()) {
        std::cerr << "WebSockets are not supported in this browser runtime" << std::endl;
        return 1;
    }

    if (uri == nullptr || uri[0] == '\0') {
        std::cerr << "Network client requires a server URI" << std::endl;
        return 1;
    }

    g_uri = uri;
    g_reset_save = reset_save;
    std::cout << "[WEB CLIENT] connecting to " << g_uri << std::endl;

    if (!glfwInit()) {
        std::cerr << "glfwInit() failed" << std::endl;
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    g_window = glfwCreateWindow(1280, 720, "Client-Web", nullptr, nullptr);
    if (!g_window) {
        std::cerr << "glfwCreateWindow() failed" << std::endl;
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(g_window);

    if (!create_shader_program()) {
        std::cerr << "Failed to initialize renderer" << std::endl;
        return 1;
    }

    EmscriptenWebSocketCreateAttributes attributes = {
        g_uri.c_str(),
        nullptr,
        EM_TRUE
    };

    g_socket = emscripten_websocket_new(&attributes);
    if (g_socket <= 0) {
        std::cerr << "Failed to create websocket for URI: " << g_uri << std::endl;
        return 1;
    }

    emscripten_websocket_set_onopen_callback(g_socket, nullptr, on_open);
    emscripten_websocket_set_onclose_callback(g_socket, nullptr, on_close);
    emscripten_websocket_set_onerror_callback(g_socket, nullptr, on_error);
    emscripten_websocket_set_onmessage_callback(g_socket, nullptr, on_message);

    emscripten_set_main_loop(idle_tick, 0, 1);
    return 0;
#endif
}
