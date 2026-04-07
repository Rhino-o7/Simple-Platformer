#pragma once

#include <corelib/net/asio.hpp>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/client.hpp>

#include <atomic>
#include <chrono>
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
    };

    struct PlayerState {
        int id = 0;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        InputState input;
    };

    inline std::string serialize_state(const PlayerState& state) {
        std::ostringstream out;
        out << "STATE " << state.id << ' ' << state.x << ' ' << state.y << ' ' << state.z;
        return out.str();
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
                clients_[hdl] = PlayerState{ id };
                endpoint_.send(hdl, "WELCOME " + std::to_string(id), websocketpp::frame::opcode::text);
            });

            endpoint_.set_close_handler([this](websocketpp::connection_hdl hdl) {
                std::lock_guard<std::mutex> lock(mutex_);
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
                if (type == "INPUT") {
                    int jump = 0;
                    in >> it->second.input.move_x >> it->second.input.move_z >> jump;
                    it->second.input.jump = jump != 0;
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

                std::lock_guard<std::mutex> lock(mutex_);
                for (auto& [hdl, state] : clients_) {
                    state.x += state.input.move_x * 20.0f * dt;
                    state.z += state.input.move_z * 20.0f * dt;
                    if (state.input.jump) {
                        state.y = 1.0f;
                    }
                    else {
                        state.y = 0.0f;
                    }

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
    };

    class GameClient {
    public:
        using client_t = websocketpp::client<websocketpp::config::asio_client>;

        bool connect(const std::string& uri) {
            if (running_) {
                return true;
            }

            endpoint_.clear_access_channels(websocketpp::log::alevel::all);
            endpoint_.clear_error_channels(websocketpp::log::elevel::all);
            endpoint_.init_asio();

            endpoint_.set_open_handler([this](websocketpp::connection_hdl hdl) {
                std::lock_guard<std::mutex> lock(mutex_);
                connection_ = hdl;
                connected_ = true;
            });

            endpoint_.set_close_handler([this](websocketpp::connection_hdl) {
                connected_ = false;
            });

            endpoint_.set_message_handler([this](websocketpp::connection_hdl, client_t::message_ptr msg) {
                std::lock_guard<std::mutex> lock(mutex_);
                last_message_ = msg->get_payload();
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
            return true;
        }

        void send_input(float move_x, float move_z, bool jump) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!connected_) {
                return;
            }

            std::ostringstream out;
            out << "INPUT " << move_x << ' ' << move_z << ' ' << (jump ? 1 : 0);
            websocketpp::lib::error_code ec;
            endpoint_.send(connection_, out.str(), websocketpp::frame::opcode::text, ec);
        }

        std::string last_message() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return last_message_;
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
    };
}
