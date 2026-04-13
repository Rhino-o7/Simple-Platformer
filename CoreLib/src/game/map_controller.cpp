#include "map_controller.hpp"
#include "game_manager.hpp"
#include "level_layout.hpp"
#include "platform.hpp"
#include "turret.hpp"

#include <data/manager.hpp>
#include <ecs/transform.hpp>
#include <corelib/net/websocket.hpp>
#include <input/keyboard.hpp>

#include "PerlinNoise.hpp"

#include <random>
#include <iostream>
#include <algorithm>
#include <fstream>

using input::Keyboard;
using Key = Keyboard::Key;

namespace {
    template<typename T>
    T* get_behaviour(vpg::ecs::Entity e) {
        auto b = vpg::ecs::get_component<vpg::ecs::Behaviour>(e);
        return b != nullptr ? dynamic_cast<T*>(b->get()) : nullptr;
    }

    int load_local_level_save() {
        const auto save_path = corelib::net::path_in_executable_directory("client_save.txt");
        std::ifstream in(save_path);
        int level = 1;
        if (in >> level && level > 0) {
            return level;
        }
        return 1;
    }

    void save_local_level(int level) {
        const auto save_path = corelib::net::path_in_executable_directory("client_save.txt");
        std::ofstream out(save_path, std::ios::trunc);
        if (!out.is_open()) {
            return;
        }

        out << std::max(1, level) << '\n';
    }
}

bool MapController::Info::serialize(memory::Stream& stream) const {
    stream.write_ref(this->player);
    stream.write_ref(this->kill_area);

    stream.write_u32((uint32_t)this->prefabs.size());
    for (const auto& entry : this->prefabs) {
        if (entry.second.get_asset() == nullptr) {
            std::cerr << "MapController::Info::serialize() failed:\n"
                      << "Missing prefab asset for key '" << entry.first << "'\n";
            return false;
        }

        stream.write_string(entry.first);
        stream.write_string(entry.second.get_asset()->get_id());
    }

    return !stream.failed();
}

bool MapController::Info::deserialize(memory::Stream& stream) {
    this->player = stream.read_ref();
    this->kill_area = stream.read_ref();

    this->prefabs.clear();
    uint32_t scene_count = stream.read_u32();
    for (uint32_t i = 0; i < scene_count; ++i) {
        auto key = stream.read_string();
        auto asset_id = stream.read_string();
        auto prefab = data::Manager::load<data::Text>(asset_id);
        if (prefab.get_asset() == nullptr) {
            std::cerr << "MapController::Info::deserialize() failed:\n"
                      << "No prefab asset found for key '" << key << "' and asset id '" << asset_id << "'\n";
            return false;
        }

        this->prefabs[key] = prefab;
    }

    return !stream.failed();
}

MapController::MapController(vpg::ecs::Entity entity, const Info& info) {
    this->prefabs = info.prefabs;

    this->kill_area = info.kill_area;
    this->entry = this->spawn_prefab("entry");
    this->exit = this->spawn_prefab("exit");

    auto collider = ecs::get_component<physics::Collider>(info.kill_area);
    if (collider != nullptr) {
        collider->on_collision.add_listener(std::bind(
            &MapController::on_kill_area_collision,
            this,
            std::placeholders::_1
        ));
    }

    collider = ecs::get_component<physics::Collider>(this->exit);
    if (collider != nullptr) {
        collider->on_collision.add_listener(std::bind(
            &MapController::on_exit_area_collision,
            this,
            std::placeholders::_1
        ));
    }

    this->player = get_behaviour<PlayerInstance>(info.player);

    this->level_num = 0;
    this->pending_respawn = false;
    this->pending_next_level = false;
    this->save_key_was_down = false;
    this->network_level = 1;
    this->network_respawn_revision = -1;
    this->level_initialized = false;

    if (corelib::net::active_client() == nullptr) {
        this->level_num = std::max(0, load_local_level_save() - 1);
    }
    this->gen_level();
}

MapController::~MapController() {
    ecs::destroy_entity(this->entry);
}

void MapController::on_kill_area_collision(const physics::Manifold& manifold) {
    (void)manifold;
    auto network = corelib::net::active_client();
    if (network != nullptr) {
        if (network->connected() && !this->pending_respawn) {
            network->send_event("RESPAWN", 0);
            this->pending_respawn = true;
        }
        return;
    }
    this->pending_respawn = true;
}

void MapController::on_exit_area_collision(const physics::Manifold& manifold) {
    (void)manifold;
    auto network = corelib::net::active_client();
    if (network != nullptr) {
        if (network->connected() && !this->pending_next_level) {
            network->send_event("NEXT_LEVEL", 1);
            this->pending_next_level = true;
        }
        return;
    }
    this->pending_next_level = true;
}

void MapController::update(float dt) {
    (void)dt;

    if (auto network = corelib::net::active_client(); network != nullptr && network->connected()) {
        corelib::net::PlayerState state;
        if (network->try_get_latest_state(state)) {
            if (state.level != this->network_level) {
                this->network_level = state.level;
                this->level_num = std::max(0, state.level - 1);
                this->gen_level();
                this->pending_next_level = false;
            }

            if (this->network_respawn_revision != -1 && state.respawn_revision != this->network_respawn_revision) {
                if (this->player != nullptr && this->player->controller != nullptr) {
                    this->player->controller->respawn(this->player->spawn_position);
                }
                this->pending_respawn = false;
            }

            this->network_respawn_revision = state.respawn_revision;
        }

        if (!this->level_initialized && network->has_server_level_layout()) {
            this->level_num = std::max(0, this->network_level - 1);
            this->gen_level();
        }
    }

    const bool save_key_down = Keyboard::is_key_pressed(Key::F5);
    if (save_key_down && !this->save_key_was_down) {
        if (auto network = corelib::net::active_client(); network != nullptr) {
            if (network->connected()) {
                network->send_event("SAVE", 0);
                std::cout << "\nProgress save requested on server.";
            }
        }
        else {
            save_local_level(this->level_num + 1);
            std::cout << "\nProgress saved locally at level " << (this->level_num + 1) << ".";
        }
    }
    this->save_key_was_down = save_key_down;

    if (this->pending_next_level && corelib::net::active_client() == nullptr) {
        this->pending_next_level = false;
        this->level_num += 1;
        this->gen_level();
        this->pending_respawn = true;
    }

    if (!this->pending_respawn) {
        return;
    }

    this->pending_respawn = false;
    if (this->player != nullptr && this->player->controller != nullptr) {
        this->player->controller->respawn(this->player->spawn_position);
    }
}

const char* MapController::get_level_name(int level_num) {
    auto level = game::level_layout::get_level_definition(level_num);
    if (level == nullptr || level->name.empty()) {
        return "Unknown";
    }

    return level->name.c_str();
}

data::Handle<data::Text> MapController::get_prefab(const std::string& key) const {
    auto it = this->prefabs.find(key);
    if (it == this->prefabs.end()) {
        return data::Handle<data::Text>(nullptr);
    }

    return it->second;
}

ecs::Entity MapController::spawn_prefab(const std::string& key) {
    auto prefab = this->get_prefab(key);
    if (prefab.get_asset() == nullptr) {
        std::cerr << "MapController::spawn_prefab() failed:\n"
                  << "No prefab mapped for key '" << key << "'\n";
        return ecs::NullEntity;
    }

    return Manager::instance(prefab);
}

void MapController::gen_level() {
    this->level_initialized = false;

    auto world = vpg::ecs::get_world();
    if (world == nullptr) {
        return;
    }

    for (auto& e : this->level) {
        Manager::destroy_instance(e);
    }
    this->level.clear();

    const auto* level_definition = game::level_layout::get_level_definition(this->level_num);
    if (level_definition == nullptr) {
        if (auto network = corelib::net::active_client(); network != nullptr) {
            return;
        }

        std::cerr << "MapController::gen_level() failed:\n"
                  << "No level definition for index " << this->level_num << "\n";
        return;
    }

    std::cout << "\nLoading level " << (this->level_num + 1)
              << " (" << level_definition->name << ")\n";

    if (corelib::net::active_client() == nullptr) {
        save_local_level(this->level_num + 1);
    }

    std::random_device rd;  // a seed source for the random number engine
    std::mt19937 gen(rd()); // mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<> distrib(1, 12346);
    const siv::PerlinNoise::seed_type seed = distrib(gen);
    siv::PerlinNoise perlin{ seed };
    std::cout << "Starting to make the level \n";
    if (this->player == nullptr || this->player->controller == nullptr) {
        return;
    }
    this->player->controller->seed = seed;

    auto exit_transform = world->entity(this->exit).try_get_mut<ecs::Transform>();
    if (exit_transform == nullptr) {
        return;
    }

    for (const auto& spawn : level_definition->spawns) {
        auto e = this->spawn_prefab(spawn.prefab);
        if (e == ecs::NullEntity) {
            continue;
        }

        auto transform = world->entity(e).try_get_mut<ecs::Transform>();
        if (transform != nullptr) {
            if (spawn.has_position) {
                transform->set_position(spawn.position);
            }
            if (spawn.has_rotation) {
                transform->set_rotation(spawn.rotation);
            }
            if (spawn.has_look_at_direction) {
                transform->look_at(transform->get_position() + spawn.look_at_direction, glm::vec3(0.0f, 1.0f, 0.0f));
            }
        }

        if (spawn.has_platform) {
            auto platform = get_behaviour<Platform>(e);
            if (platform != nullptr) {
                platform->from = spawn.platform_from;
                platform->to = spawn.platform_to;
                platform->set_center(spawn.platform_center);
                if (spawn.platform_has_speed) {
                    platform->speed = spawn.platform_speed;
                }
                platform->speed *= spawn.platform_speed_multiplier;
            }
        }

        if (spawn.has_turret) {
            auto turret = get_behaviour<Turret>(e);
            if (turret != nullptr) {
                if (spawn.turret_has_delay) {
                    turret->delay = spawn.turret_delay;
                }
                if (spawn.turret_has_speed) {
                    turret->speed = spawn.turret_speed;
                }
            }
        }

        this->level.push_back(e);
    }

    if (level_definition->has_exit) {
        exit_transform->set_position(level_definition->exit_position);
    }

    if (level_definition->has_player_distance) {
        player->controller->SetDistance(level_definition->player_distance);
    }

    if (level_definition->has_player_timer) {
        player->controller->timer = level_definition->player_timer;
        if (auto network = corelib::net::active_client(); network != nullptr && network->connected()) {
            network->send_event("SET_TIMER", level_definition->player_timer);
        }
    }

    player->controller->level = this->level_num + 1;
    this->player->controller->respawn(this->player->spawn_position);
    this->level_initialized = true;
    std::cout << "\n End loading level \n";
}








