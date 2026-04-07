#pragma once

#include <corelib/net/asio.hpp>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/client.hpp>

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

namespace corelib::net {
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
        InputState input;
    };

    inline std::string serialize_state(const PlayerState& state) {
        std::ostringstream out;
        out << "STATE "
            << state.id << ' '
            << state.x << ' '
            << state.y << ' '
            << state.z << ' '
            << state.wind << ' '
            << state.health << ' '
            << state.seconds;
        return out.str();
    }

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
        }
        return !in.fail();
    }

    class GameServer {
    public:
        using server_t = websocketpp::server<websocketpp::config::asio>;

        bool start(uint16_t port) {
            if (running_) {
                return true;
            }

            endpoint_.clear_access_channels(websocketpp::log::alevel::all);
            endpoint_.clear_error_channels(websocketpp::log::elevel::all);
            endpoint_.init_asio();
            endpoint_.set_reuse_addr(true);

            endpoint_.set_open_handler([this](websocketpp::connection_hdl hdl) {
                std::lock_guard<std::mutex> lock(mutex_);
                const int id = ++next_id_;
                PlayerState state{};
                state.id = id;
                state.y = 15.0f;
                state.on_ground = false;
                state.ground_y = 0.0f;
                clients_[hdl] = state;
                const std::string welcome = "WELCOME " + std::to_string(id);
                std::cout << "[SERVER -> CLIENT " << id << "] " << welcome << '\n';
                endpoint_.send(hdl, welcome, websocketpp::frame::opcode::text);
                std::cout << "[SERVER] client connected id=" << id << '\n';
            });

            endpoint_.set_close_handler([this](websocketpp::connection_hdl hdl) {
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = clients_.find(hdl);
                if (it != clients_.end()) {
                    std::cout << "[SERVER] client disconnected id=" << it->second.id << '\n';
                }
                clients_.erase(hdl);
            });

            endpoint_.set_message_handler([this](websocketpp::connection_hdl hdl, server_t::message_ptr msg) {
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = clients_.find(hdl);
                if (it == clients_.end()) {
                    return;
                }

                std::istringstream in(msg->get_payload());
                std::string type;
                in >> type;
                if (type != "INPUT") {
                    std::cout << "[CLIENT " << it->second.id << " -> SERVER] " << msg->get_payload() << '\n';
                }
                if (type == "INPUT") {
                    int jump = 0;
                    int sprint = 0;
                    in >> it->second.input.move_x >> it->second.input.move_z >> jump >> sprint;
                    it->second.input.jump = jump != 0;
                    it->second.input.sprint = sprint != 0;
                }
                else if (type == "SPAWN") {
                    in >> it->second.x >> it->second.y >> it->second.z;
                    it->second.vy = 0.0f;
                    it->second.on_ground = false;
                    it->second.ground_y = it->second.y;
                    std::cout << "[SERVER] spawn set for client " << it->second.id
                              << " at (" << it->second.x << ", " << it->second.y << ", " << it->second.z << ")\n";
                }
            });

            try {
                endpoint_.listen(port);
                endpoint_.start_accept();
            }
            catch (const std::exception& ex) {
                std::cerr << "GameServer start failed: " << ex.what() << '\n';
                return false;
            }

            running_ = true;
            net_thread_ = std::thread([this]() { endpoint_.run(); });
            logic_thread_ = std::thread([this]() { logic_loop(); });
            return true;
        }

        void stop() {
            if (!running_) {
                return;
            }

            running_ = false;
            try {
                endpoint_.stop_listening();
                endpoint_.stop();
            }
            catch (...) {
            }

            if (logic_thread_.joinable()) {
                logic_thread_.join();
            }
            if (net_thread_.joinable()) {
                net_thread_.join();
            }
        }

        ~GameServer() {
            stop();
        }

    private:
        void logic_loop() {
            using clock = std::chrono::steady_clock;
            auto last = clock::now();

            while (running_) {
                auto now = clock::now();
                const float dt = std::chrono::duration<float>(now - last).count();
                last = now;
                second_accumulator_ += dt;
                int elapsed_seconds = 0;
                while (second_accumulator_ >= 1.0f) {
                    ++elapsed_seconds;
                    second_accumulator_ -= 1.0f;
                }

                std::lock_guard<std::mutex> lock(mutex_);
                for (auto& [hdl, state] : clients_) {
                    if (elapsed_seconds > 0) {
                        state.seconds = std::max(0, state.seconds - elapsed_seconds);
                    }

                    // Movement is client-authoritative for now.
                    // Server keeps session/time state only.
                    state.wind = 0.0f;

                    endpoint_.send(hdl, serialize_state(state), websocketpp::frame::opcode::text);
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
        }

        struct HdlOwnerLess {
            bool operator()(const websocketpp::connection_hdl& a, const websocketpp::connection_hdl& b) const {
                return a.owner_before(b);
            }
        };

        server_t endpoint_;
        std::atomic<bool> running_ = false;
        std::thread net_thread_;
        std::thread logic_thread_;
        std::mutex mutex_;
        std::map<websocketpp::connection_hdl, PlayerState, HdlOwnerLess> clients_;
        int next_id_ = 0;
        float second_accumulator_ = 0.0f;
    };

    class GameClient {
    public:
        using client_t = websocketpp::client<websocketpp::config::asio_client>;

        bool connect(const std::string& uri) {
            if (running_) {
                return true;
            }

            {
                std::lock_guard<std::mutex> guard(connection_mutex_);
                connection_attempt_finished_ = false;
            }

            endpoint_.clear_access_channels(websocketpp::log::alevel::all);
            endpoint_.clear_error_channels(websocketpp::log::elevel::all);
            endpoint_.init_asio();

            endpoint_.set_open_handler([this](websocketpp::connection_hdl hdl) {
                std::lock_guard<std::mutex> lock(mutex_);
                connection_ = hdl;
                connected_ = true;
                std::cout << "[CLIENT] connected\n";

                {
                    std::lock_guard<std::mutex> guard(connection_mutex_);
                    connection_attempt_finished_ = true;
                }
                connection_cv_.notify_all();
            });

            endpoint_.set_close_handler([this](websocketpp::connection_hdl) {
                connected_ = false;
                std::cout << "[CLIENT] disconnected\n";

                {
                    std::lock_guard<std::mutex> guard(connection_mutex_);
                    connection_attempt_finished_ = true;
                }
                connection_cv_.notify_all();
            });

            endpoint_.set_fail_handler([this](websocketpp::connection_hdl) {
                connected_ = false;
                std::cout << "[CLIENT] connection failed\n";

                {
                    std::lock_guard<std::mutex> guard(connection_mutex_);
                    connection_attempt_finished_ = true;
                }
                connection_cv_.notify_all();
            });

            endpoint_.set_message_handler([this](websocketpp::connection_hdl, client_t::message_ptr msg) {
                std::lock_guard<std::mutex> lock(mutex_);
                last_message_ = msg->get_payload();
                std::istringstream in(last_message_);
                std::string type;
                in >> type;
                if (type != "STATE") {
                    std::cout << "[SERVER -> CLIENT] " << last_message_ << '\n';
                }
                if (type == "WELCOME") {
                    in >> client_id_;
                    return;
                }

                PlayerState parsed;
                if (parse_state_message(last_message_, parsed)) {
                    latest_state_ = parsed;
                    has_state_ = true;
                }
            });

            websocketpp::lib::error_code ec;
            auto connection = endpoint_.get_connection(uri, ec);
            if (ec) {
                std::cerr << "GameClient connect failed: " << ec.message() << '\n';
                return false;
            }

            endpoint_.connect(connection);
            running_ = true;
            thread_ = std::thread([this]() { endpoint_.run(); });

            {
                std::unique_lock<std::mutex> lock(connection_mutex_);
                connection_cv_.wait_for(lock, std::chrono::seconds(5), [this]() {
                    return connection_attempt_finished_;
                });
            }

            if (!connected_) {
                stop();
                return false;
            }

            return true;
        }

        void send_input(float move_x, float move_z, bool jump, bool sprint) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!connected_) {
                return;
            }

            InputState input;
            input.move_x = move_x;
            input.move_z = move_z;
            input.jump = jump;
            input.sprint = sprint;

            auto now = std::chrono::steady_clock::now();
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
            websocketpp::lib::error_code ec;
            endpoint_.send(connection_, out.str(), websocketpp::frame::opcode::text, ec);
            if (!ec) {
                last_sent_input_ = input;
                has_last_sent_input_ = true;
                last_send_time_ = now;
            }
        }

        void send_spawn(float x, float y, float z) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!connected_) {
                return;
            }

            std::ostringstream out;
            out << "SPAWN " << x << ' ' << y << ' ' << z;
            websocketpp::lib::error_code ec;
            endpoint_.send(connection_, out.str(), websocketpp::frame::opcode::text, ec);
            if (!ec) {
                std::cout << "[CLIENT -> SERVER] " << out.str() << '\n';
            }
        }

        std::string last_message() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return last_message_;
        }

        bool connected() const {
            return connected_;
        }

        bool try_get_latest_state(PlayerState& out_state) const {
            std::lock_guard<std::mutex> lock(mutex_);
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
            websocketpp::lib::error_code ec;
            if (connected_) {
                endpoint_.close(connection_, websocketpp::close::status::normal, "shutdown", ec);
                connected_ = false;
            }
            endpoint_.stop();
            if (thread_.joinable()) {
                thread_.join();
            }
        }

        ~GameClient() {
            stop();
        }

    private:
        client_t endpoint_;
        mutable std::mutex mutex_;
        websocketpp::connection_hdl connection_;
        std::thread thread_;
        std::atomic<bool> running_ = false;
        std::atomic<bool> connected_ = false;
        std::string last_message_;
        PlayerState latest_state_;
        bool has_state_ = false;
        int client_id_ = -1;
        std::mutex connection_mutex_;
        std::condition_variable connection_cv_;
        bool connection_attempt_finished_ = false;
        InputState last_sent_input_;
        bool has_last_sent_input_ = false;
        std::chrono::steady_clock::time_point last_send_time_{};
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
