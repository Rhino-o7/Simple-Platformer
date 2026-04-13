#pragma once

#include <emscripten/websocket.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

namespace corelib::net {
    inline std::filesystem::path executable_directory() {
        return std::filesystem::current_path();
    }

    inline std::string path_in_executable_directory(const char* filename) {
        return (executable_directory() / filename).string();
    }

    struct InputState {
        float move_x = 0.0f;
        float move_z = 0.0f;
        bool jump = false;
        bool sprint = false;
    };

    struct PlayerState {
        int id = 0;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float vy = 0.0f;
        bool on_ground = true;
        float ground_y = 0.0f;
        float wind = 0.0f;
        int health = 3;
        int seconds = 120;
        int level = 1;
        int respawn_revision = 0;
        std::string profile_id;
        InputState input;
    };

    inline bool parse_state_message(const std::string& payload, PlayerState& out_state) {
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

    class GameClient {
    public:
        bool connect(const std::string& uri) {
            if (running_) {
                return true;
            }

            if (!emscripten_websocket_is_supported()) {
                std::cerr << "[CLIENT] websocket not supported\n";
                return false;
            }

            std::string connect_uri = uri;
#ifdef __EMSCRIPTEN__
            const int is_https = EM_ASM_INT({ return window.location.protocol === 'https:' ? 1 : 0; });
            if (is_https && connect_uri.rfind("ws://", 0) == 0) {
                connect_uri.replace(0, 5, "wss://");
            }
#endif

            std::cout << "[CLIENT] connecting to " << connect_uri << '\n';

            EmscriptenWebSocketCreateAttributes attributes = {
                connect_uri.c_str(),
                nullptr,
                EM_TRUE
            };

            socket_ = emscripten_websocket_new(&attributes);
            if (socket_ <= 0) {
                std::cerr << "[CLIENT] websocket create failed\n";
                return false;
            }

            running_ = true;
            emscripten_websocket_set_onopen_callback(socket_, this, &GameClient::on_open);
            emscripten_websocket_set_onclose_callback(socket_, this, &GameClient::on_close);
            emscripten_websocket_set_onerror_callback(socket_, this, &GameClient::on_error);
            emscripten_websocket_set_onmessage_callback(socket_, this, &GameClient::on_message);
            return true;
        }

        void send_input(float move_x, float move_z, bool jump, bool sprint) {
            if (!connected_) {
                return;
            }

            InputState input;
            input.move_x = move_x;
            input.move_z = move_z;
            input.jump = jump;
            input.sprint = sprint;

            const auto now = std::chrono::steady_clock::now();
            const bool changed = !has_last_sent_input_
                || input.move_x != last_sent_input_.move_x
                || input.move_z != last_sent_input_.move_z
                || input.jump != last_sent_input_.jump
                || input.sprint != last_sent_input_.sprint;

            if (!changed && (now - last_send_time_) < std::chrono::milliseconds(50)) {
                return;
            }

            std::ostringstream out;
            out << "INPUT " << move_x << ' ' << move_z << ' ' << (jump ? 1 : 0) << ' ' << (sprint ? 1 : 0);
            emscripten_websocket_send_utf8_text(socket_, out.str().c_str());

            last_sent_input_ = input;
            has_last_sent_input_ = true;
            last_send_time_ = now;
        }

        void send_spawn(float x, float y, float z) {
            if (!connected_) {
                return;
            }

            std::ostringstream out;
            out << "SPAWN " << x << ' ' << y << ' ' << z;
            emscripten_websocket_send_utf8_text(socket_, out.str().c_str());
        }

        void send_event(const std::string& event_type, int value = 0) {
            if (!connected_) {
                return;
            }

            std::ostringstream out;
            out << "EVENT " << event_type << ' ' << value;
            emscripten_websocket_send_utf8_text(socket_, out.str().c_str());
        }

        std::string last_message() const {
            return last_message_;
        }

        bool connected() const {
            return connected_;
        }

        bool try_get_latest_state(PlayerState& out_state) const {
            if (!has_state_) {
                return false;
            }

            out_state = latest_state_;
            return true;
        }

        void stop() {
            if (!running_) {
                return;
            }

            running_ = false;
            if (connected_) {
                emscripten_websocket_close(socket_, 1000, "shutdown");
                connected_ = false;
            }

            if (socket_ > 0) {
                emscripten_websocket_delete(socket_);
                socket_ = 0;
            }
        }

        ~GameClient() {
            stop();
        }

    private:
        static EM_BOOL on_open(int, const EmscriptenWebSocketOpenEvent*, void* user_data) {
            auto* self = static_cast<GameClient*>(user_data);
            self->connected_ = true;
            std::cout << "[CLIENT] connected\n";

            std::ostringstream hello;
            hello << "HELLO " << self->profile_id_;
            emscripten_websocket_send_utf8_text(self->socket_, hello.str().c_str());
            return EM_TRUE;
        }

        static EM_BOOL on_close(int, const EmscriptenWebSocketCloseEvent*, void* user_data) {
            auto* self = static_cast<GameClient*>(user_data);
            self->connected_ = false;
            std::cout << "[CLIENT] disconnected\n";
            return EM_TRUE;
        }

        static EM_BOOL on_error(int, const EmscriptenWebSocketErrorEvent*, void* user_data) {
            auto* self = static_cast<GameClient*>(user_data);
            self->connected_ = false;
            std::cout << "[CLIENT] websocket error\n";
            return EM_TRUE;
        }

        static EM_BOOL on_message(int, const EmscriptenWebSocketMessageEvent* msg, void* user_data) {
            auto* self = static_cast<GameClient*>(user_data);
            if (!msg->isText || msg->data == nullptr || msg->numBytes == 0) {
                return EM_TRUE;
            }

            self->last_message_ = std::string(reinterpret_cast<const char*>(msg->data), static_cast<size_t>(msg->numBytes));

            std::istringstream in(self->last_message_);
            std::string type;
            in >> type;
            if (type == "WELCOME") {
                in >> self->client_id_;
                return EM_TRUE;
            }

            PlayerState parsed;
            if (parse_state_message(self->last_message_, parsed)) {
                self->latest_state_ = parsed;
                self->has_state_ = true;
            }

            return EM_TRUE;
        }

        EMSCRIPTEN_WEBSOCKET_T socket_ = 0;
        bool running_ = false;
        bool connected_ = false;
        std::string last_message_;
        PlayerState latest_state_;
        bool has_state_ = false;
        int client_id_ = -1;
        InputState last_sent_input_;
        bool has_last_sent_input_ = false;
        std::chrono::steady_clock::time_point last_send_time_{};
        std::string profile_id_ = "web_player";
    };

    inline GameClient*& active_client_slot() {
        static GameClient* client = nullptr;
        return client;
    }

    inline void set_active_client(GameClient* client) {
        active_client_slot() = client;
    }

    inline GameClient* active_client() {
        return active_client_slot();
    }
}
