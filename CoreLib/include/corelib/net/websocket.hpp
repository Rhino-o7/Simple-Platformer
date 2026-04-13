#pragma once

#include <corelib/net/asio.hpp>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/client.hpp>

#include <atomic>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <cctype>
#include <fstream>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace corelib::net {
    inline std::filesystem::path executable_directory() {
#ifdef _WIN32
        char module_path[MAX_PATH] = {};
        const DWORD size = GetModuleFileNameA(nullptr, module_path, MAX_PATH);
        if (size > 0) {
            return std::filesystem::path(std::string(module_path, size)).parent_path();
        }
#endif
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

    inline std::string serialize_state(const PlayerState& state) {
        std::ostringstream out;
        out << "STATE "
            << state.id << ' '
            << state.x << ' '
            << state.y << ' '
            << state.z << ' '
            << state.wind << ' '
            << state.health << ' '
            << state.seconds << ' '
            << state.level << ' '
            << state.respawn_revision;
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

    class GameServer {
    public:
        using server_t = websocketpp::server<websocketpp::config::asio>;

        bool start(uint16_t port) {
            if (running_) {
                return true;
            }

            load_saved_progress();
            load_server_assets();

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
                    persist_client_progress(it->second);
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
                if (type == "INPUT") {
                    int jump = 0;
                    int sprint = 0;
                    in >> it->second.input.move_x >> it->second.input.move_z >> jump >> sprint;
                    it->second.input.jump = jump != 0;
                    it->second.input.sprint = sprint != 0;
                }
                else if (type == "HELLO") {
                    std::string profile_id;
                    in >> profile_id;
                    if (profile_id.empty()) {
                        return;
                    }

                    it->second.profile_id = profile_id;
                    auto saved = saved_levels_.find(profile_id);
                    if (saved != saved_levels_.end()) {
                        it->second.level = saved->second;
                        std::cout << "[SERVER] loaded save for '" << profile_id
                                  << "' at level " << it->second.level << '\n';
                    }

                    if (!level_layout_payload_.empty()) {
                        endpoint_.send(hdl, std::string("ASSET LEVEL_LAYOUT ") + level_layout_payload_, websocketpp::frame::opcode::text);
                    }
                }
                else if (type == "SPAWN") {
                    std::cout << "[CLIENT " << it->second.id << " -> SERVER] " << msg->get_payload() << std::endl;
                    in >> it->second.x >> it->second.y >> it->second.z;
                    it->second.vy = 0.0f;
                    it->second.on_ground = false;
                    it->second.ground_y = it->second.y;
                    std::cout << "[SERVER] spawn set for client " << it->second.id
                              << " at (" << it->second.x << ", " << it->second.y << ", " << it->second.z << ")\n";
                }
                else if (type == "EVENT") {
                    std::cout << "[CLIENT " << it->second.id << " -> SERVER] " << msg->get_payload() << std::endl;
                    std::string event_type;
                    int value = 0;
                    in >> event_type >> value;

                    if (event_type == "DAMAGE") {
                        const int damage = std::max(1, value);
                        it->second.health -= damage;
                        if (it->second.health <= 0) {
                            it->second.health = 3;
                            it->second.respawn_revision += 1;
                        }
                    }
                    else if (event_type == "RESPAWN") {
                        it->second.respawn_revision += 1;
                    }
                    else if (event_type == "NEXT_LEVEL") {
                        it->second.level += 1;
                        it->second.respawn_revision += 1;
                    }
                    else if (event_type == "SET_TIMER") {
                        if (value > 0) {
                            it->second.seconds = value;
                        }
                    }
                    else if (event_type == "RESET_SAVE") {
                        it->second.level = 1;
                        it->second.health = 3;
                        it->second.seconds = 120;
                        it->second.respawn_revision += 1;
                        persist_client_progress(it->second);
                    }
                    else if (event_type == "SAVE") {
                        persist_client_progress(it->second);
                    }

                    std::cout << "[CLIENT " << it->second.id << " -> SERVER] EVENT "
                              << event_type << " " << value << "\n";
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
            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (const auto& [_, state] : clients_) {
                    persist_client_progress(state);
                }
            }
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
        static bool read_text_file(const std::filesystem::path& path, std::string& out_content) {
            std::ifstream file(path, std::ios::binary);
            if (!file.is_open()) {
                return false;
            }

            out_content.assign((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
            return !out_content.empty();
        }

        void load_server_assets() {
            level_layout_payload_.clear();

            const auto exe_dir = executable_directory();
            std::cout << "[SERVER] asset lookup base: '" << exe_dir.string() << "'\n";

            auto dir = exe_dir;
            while (!dir.empty()) {
                const std::vector<std::filesystem::path> candidates = {
                    dir / "Client" / "data" / "level" / "levels.json",
                    dir / "data" / "level" / "levels.json"
                };

                for (const auto& path : candidates) {
                    if (read_text_file(path, level_layout_payload_)) {
                        std::cout << "[SERVER] loaded level layout asset from '" << path.string() << "'\n";
                        return;
                    }
                }

                if (dir == dir.root_path()) {
                    break;
                }

                dir = dir.parent_path();
            }

            std::cerr << "[SERVER] failed to load level layout asset (levels.json)\n";
        }

        void load_saved_progress() {
            std::lock_guard<std::mutex> lock(mutex_);
            saved_levels_.clear();

            std::ifstream file(save_file_path_);
            if (!file.is_open()) {
                return;
            }

            std::string profile;
            int level = 1;
            while (file >> profile >> level) {
                if (!profile.empty() && level > 0) {
                    saved_levels_[profile] = level;
                }
            }
        }

        void flush_saved_progress() {
            std::ofstream file(save_file_path_, std::ios::trunc);
            if (!file.is_open()) {
                return;
            }

            for (const auto& [profile, level] : saved_levels_) {
                file << profile << ' ' << level << '\n';
            }
        }

        void persist_client_progress(const PlayerState& state) {
            if (state.profile_id.empty()) {
                return;
            }

            const int safe_level = std::max(1, state.level);
            saved_levels_[state.profile_id] = safe_level;
            flush_saved_progress();
            std::cout << "[SERVER] saved progress for '" << state.profile_id
                      << "' at level " << safe_level << '\n';
        }

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

                {
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
        std::map<std::string, int> saved_levels_;
        int next_id_ = 0;
        float second_accumulator_ = 0.0f;
        std::string level_layout_payload_;
        const std::string save_file_path_ = path_in_executable_directory("server_saves.txt");
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
                connection_open_time_ = std::chrono::steady_clock::now();
                last_server_message_time_ = connection_open_time_;
                has_seen_server_message_ = false;
                has_server_level_layout_ = false;
                std::cout << "[CLIENT] connected\n";

                std::ostringstream hello;
                hello << "HELLO " << profile_id_;
                websocketpp::lib::error_code ec;
                endpoint_.send(connection_, hello.str(), websocketpp::frame::opcode::text, ec);

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
                last_server_message_time_ = std::chrono::steady_clock::now();
                has_seen_server_message_ = true;
                const std::string asset_prefix = "ASSET LEVEL_LAYOUT ";
                if (last_message_.rfind(asset_prefix, 0) == 0) {
                    server_level_layout_ = last_message_.substr(asset_prefix.size());
                    has_server_level_layout_ = !server_level_layout_.empty();
                    return;
                }

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

        void send_event(const std::string& event_type, int value = 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!connected_) {
                return;
            }

            std::ostringstream out;
            out << "EVENT " << event_type << ' ' << value;
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

        bool is_connection_alive(
            std::chrono::milliseconds timeout = std::chrono::milliseconds(3000),
            std::chrono::milliseconds initial_grace = std::chrono::milliseconds(20000)) const {
            if (!connected_) {
                return false;
            }

            std::lock_guard<std::mutex> lock(mutex_);
            const auto now = std::chrono::steady_clock::now();
            if (!has_seen_server_message_) {
                return (now - connection_open_time_) <= initial_grace;
            }

            return (now - last_server_message_time_) <= timeout;
        }

        bool try_get_latest_state(PlayerState& out_state) const {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!has_state_) {
                return false;
            }

            out_state = latest_state_;
            return true;
        }

        bool has_server_level_layout() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return has_server_level_layout_;
        }

        std::string server_level_layout() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return server_level_layout_;
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
        static std::string default_profile_id() {
            auto read_env = [](const char* name) {
                std::string value;
#ifdef _WIN32
                char* buffer = nullptr;
                size_t len = 0;
                if (_dupenv_s(&buffer, &len, name) == 0 && buffer != nullptr) {
                    value = buffer;
                    free(buffer);
                }
#else
                if (const char* env = std::getenv(name)) {
                    value = env;
                }
#endif
                return value;
            };

            std::string user;
            user = read_env("USERNAME");
            if (user.empty()) {
                user = read_env("USER");
            }

            if (user.empty()) {
                user = "player";
            }

            std::string host;
            host = read_env("COMPUTERNAME");
            if (host.empty()) {
                host = read_env("HOSTNAME");
            }

            for (char& c : user) {
                if (std::isspace(static_cast<unsigned char>(c))) {
                    c = '_';
                }
            }

            for (char& c : host) {
                if (std::isspace(static_cast<unsigned char>(c))) {
                    c = '_';
                }
            }

            if (!host.empty()) {
                return user + "@" + host;
            }

            return user;
        }

        client_t endpoint_;
        mutable std::mutex mutex_;
        websocketpp::connection_hdl connection_;
        std::thread thread_;
        std::atomic<bool> running_ = false;
        std::atomic<bool> connected_ = false;
        std::string last_message_;
        PlayerState latest_state_;
        bool has_state_ = false;
        std::string server_level_layout_;
        bool has_server_level_layout_ = false;
        int client_id_ = -1;
        std::mutex connection_mutex_;
        std::condition_variable connection_cv_;
        bool connection_attempt_finished_ = false;
        InputState last_sent_input_;
        bool has_last_sent_input_ = false;
        std::chrono::steady_clock::time_point last_send_time_{};
        std::chrono::steady_clock::time_point connection_open_time_{};
        std::chrono::steady_clock::time_point last_server_message_time_{};
        bool has_seen_server_message_ = false;
        std::string profile_id_ = default_profile_id();
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
