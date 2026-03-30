#include "map_controller.hpp"
#include "game_manager.hpp"
#include "jumper.hpp"
#include "platform.hpp"
#include "turret.hpp"
#include "firetrap.hpp"
#include "smoke.hpp"
#include "firespread.hpp"
#include <config.hpp>
#include <data/manager.hpp>
#include <gl/renderer.hpp>

#include <ecs/transform.hpp>

#include "PerlinNoise.hpp"

#include <cctype>
#include <cmath>
#include <random>
#include <iostream>

namespace {
    struct SpawnDefinition {
        std::string scene;

        bool has_position = false;
        glm::vec3 position = {};

        bool has_rotation = false;
        glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

        bool has_look_at_direction = false;
        glm::vec3 look_at_direction = {};

        bool has_platform = false;
        glm::vec3 platform_from = {};
        glm::vec3 platform_to = {};
        glm::vec3 platform_center = {};
        bool platform_has_speed = false;
        float platform_speed = 0.0f;
        float platform_speed_multiplier = 1.0f;

        bool has_turret = false;
        bool turret_has_delay = false;
        float turret_delay = 0.0f;
        bool turret_has_speed = false;
        float turret_speed = 0.0f;
    };

    struct LevelDefinition {
        std::string name;
        std::vector<SpawnDefinition> spawns;

        bool has_exit = false;
        glm::vec3 exit_position = {};

        bool has_player_distance = false;
        float player_distance = 0.0f;

        bool has_player_timer = false;
        int player_timer = 0;
    };

    std::vector<LevelDefinition> g_level_definitions;
    bool g_level_definitions_loaded = false;

    class JsonCursor {
    public:
        explicit JsonCursor(const std::string& text) : text(text), index(0) {
        }

        bool parse_level_layout(std::vector<LevelDefinition>& out_levels, std::string& out_error) {
            skip_whitespace();
            if (!consume('{')) {
                out_error = "Expected '{' at root";
                return false;
            }

            bool has_levels = false;
            while (index < text.size()) {
                std::string key;
                if (!parse_string(key)) {
                    out_error = "Expected string key in root object";
                    return false;
                }

                if (!consume(':')) {
                    out_error = "Expected ':' after root key";
                    return false;
                }

                if (key == "levels") {
                    if (!parse_levels(out_levels, out_error)) {
                        return false;
                    }
                    has_levels = true;
                }
                else {
                    if (!skip_value()) {
                        out_error = "Couldn't skip unknown root value";
                        return false;
                    }
                }

                skip_whitespace();
                if (consume('}')) {
                    break;
                }

                if (!consume(',')) {
                    out_error = "Expected ',' or '}' in root object";
                    return false;
                }
            }

            if (!has_levels || out_levels.empty()) {
                out_error = "Layout must contain a non-empty 'levels' array";
                return false;
            }

            return true;
        }

    private:
        bool parse_levels(std::vector<LevelDefinition>& out_levels, std::string& out_error) {
            if (!consume('[')) {
                out_error = "Expected '[' for levels array";
                return false;
            }

            skip_whitespace();
            if (consume(']')) {
                return true;
            }

            while (index < text.size()) {
                LevelDefinition level;
                if (!parse_level(level, out_error)) {
                    return false;
                }

                if (level.name.empty()) {
                    level.name = "Level " + std::to_string(out_levels.size() + 1);
                }

                out_levels.push_back(std::move(level));

                skip_whitespace();
                if (consume(']')) {
                    break;
                }

                if (!consume(',')) {
                    out_error = "Expected ',' or ']' in levels array";
                    return false;
                }
            }

            return true;
        }

        bool parse_level(LevelDefinition& out_level, std::string& out_error) {
            if (!consume('{')) {
                out_error = "Expected '{' for level object";
                return false;
            }

            while (index < text.size()) {
                std::string key;
                if (!parse_string(key)) {
                    out_error = "Expected level key";
                    return false;
                }

                if (!consume(':')) {
                    out_error = "Expected ':' after level key";
                    return false;
                }

                if (key == "name") {
                    if (!parse_string(out_level.name)) {
                        out_error = "Expected level name string";
                        return false;
                    }
                }
                else if (key == "exit") {
                    if (!parse_vec3(out_level.exit_position)) {
                        out_error = "Expected exit vec3";
                        return false;
                    }
                    out_level.has_exit = true;
                }
                else if (key == "playerDistance") {
                    double value;
                    if (!parse_number(value)) {
                        out_error = "Expected numeric playerDistance";
                        return false;
                    }
                    out_level.player_distance = (float)value;
                    out_level.has_player_distance = true;
                }
                else if (key == "playerTimer") {
                    double value;
                    if (!parse_number(value)) {
                        out_error = "Expected numeric playerTimer";
                        return false;
                    }
                    out_level.player_timer = (int)value;
                    out_level.has_player_timer = true;
                }
                else if (key == "spawns") {
                    if (!parse_spawns(out_level.spawns, out_error)) {
                        return false;
                    }
                }
                else {
                    if (!skip_value()) {
                        out_error = "Couldn't skip unknown level value";
                        return false;
                    }
                }

                skip_whitespace();
                if (consume('}')) {
                    break;
                }

                if (!consume(',')) {
                    out_error = "Expected ',' or '}' in level object";
                    return false;
                }
            }

            return true;
        }

        bool parse_spawns(std::vector<SpawnDefinition>& out_spawns, std::string& out_error) {
            if (!consume('[')) {
                out_error = "Expected '[' for spawns array";
                return false;
            }

            skip_whitespace();
            if (consume(']')) {
                return true;
            }

            while (index < text.size()) {
                SpawnDefinition spawn;
                if (!parse_spawn(spawn, out_error)) {
                    return false;
                }

                if (spawn.scene.empty()) {
                    out_error = "Spawn object is missing required 'scene'";
                    return false;
                }

                out_spawns.push_back(std::move(spawn));

                skip_whitespace();
                if (consume(']')) {
                    break;
                }

                if (!consume(',')) {
                    out_error = "Expected ',' or ']' in spawns array";
                    return false;
                }
            }

            return true;
        }

        bool parse_spawn(SpawnDefinition& out_spawn, std::string& out_error) {
            if (!consume('{')) {
                out_error = "Expected '{' for spawn object";
                return false;
            }

            while (index < text.size()) {
                std::string key;
                if (!parse_string(key)) {
                    out_error = "Expected spawn key";
                    return false;
                }

                if (!consume(':')) {
                    out_error = "Expected ':' after spawn key";
                    return false;
                }

                if (key == "scene") {
                    if (!parse_string(out_spawn.scene)) {
                        out_error = "Expected spawn scene string";
                        return false;
                    }
                }
                else if (key == "position") {
                    if (!parse_vec3(out_spawn.position)) {
                        out_error = "Expected position vec3";
                        return false;
                    }
                    out_spawn.has_position = true;
                }
                else if (key == "rotation") {
                    if (!parse_quat(out_spawn.rotation)) {
                        out_error = "Expected rotation quat [w,x,y,z]";
                        return false;
                    }
                    out_spawn.has_rotation = true;
                }
                else if (key == "lookAtDirection") {
                    if (!parse_vec3(out_spawn.look_at_direction)) {
                        out_error = "Expected lookAtDirection vec3";
                        return false;
                    }
                    out_spawn.has_look_at_direction = true;
                }
                else if (key == "platform") {
                    if (!parse_platform(out_spawn, out_error)) {
                        return false;
                    }
                }
                else if (key == "turret") {
                    if (!parse_turret(out_spawn, out_error)) {
                        return false;
                    }
                }
                else {
                    if (!skip_value()) {
                        out_error = "Couldn't skip unknown spawn value";
                        return false;
                    }
                }

                skip_whitespace();
                if (consume('}')) {
                    break;
                }

                if (!consume(',')) {
                    out_error = "Expected ',' or '}' in spawn object";
                    return false;
                }
            }

            return true;
        }

        bool parse_platform(SpawnDefinition& out_spawn, std::string& out_error) {
            if (!consume('{')) {
                out_error = "Expected '{' for platform object";
                return false;
            }

            out_spawn.has_platform = true;
            while (index < text.size()) {
                std::string key;
                if (!parse_string(key)) {
                    out_error = "Expected platform key";
                    return false;
                }

                if (!consume(':')) {
                    out_error = "Expected ':' after platform key";
                    return false;
                }

                if (key == "from") {
                    if (!parse_vec3(out_spawn.platform_from)) {
                        out_error = "Expected platform.from vec3";
                        return false;
                    }
                }
                else if (key == "to") {
                    if (!parse_vec3(out_spawn.platform_to)) {
                        out_error = "Expected platform.to vec3";
                        return false;
                    }
                }
                else if (key == "center") {
                    if (!parse_vec3(out_spawn.platform_center)) {
                        out_error = "Expected platform.center vec3";
                        return false;
                    }
                }
                else if (key == "speed") {
                    double value;
                    if (!parse_number(value)) {
                        out_error = "Expected numeric platform.speed";
                        return false;
                    }
                    out_spawn.platform_has_speed = true;
                    out_spawn.platform_speed = (float)value;
                }
                else if (key == "speedMultiplier") {
                    double value;
                    if (!parse_number(value)) {
                        out_error = "Expected numeric platform.speedMultiplier";
                        return false;
                    }
                    out_spawn.platform_speed_multiplier = (float)value;
                }
                else {
                    if (!skip_value()) {
                        out_error = "Couldn't skip unknown platform value";
                        return false;
                    }
                }

                skip_whitespace();
                if (consume('}')) {
                    break;
                }

                if (!consume(',')) {
                    out_error = "Expected ',' or '}' in platform object";
                    return false;
                }
            }

            return true;
        }

        bool parse_turret(SpawnDefinition& out_spawn, std::string& out_error) {
            if (!consume('{')) {
                out_error = "Expected '{' for turret object";
                return false;
            }

            out_spawn.has_turret = true;
            while (index < text.size()) {
                std::string key;
                if (!parse_string(key)) {
                    out_error = "Expected turret key";
                    return false;
                }

                if (!consume(':')) {
                    out_error = "Expected ':' after turret key";
                    return false;
                }

                if (key == "delay") {
                    double value;
                    if (!parse_number(value)) {
                        out_error = "Expected numeric turret.delay";
                        return false;
                    }
                    out_spawn.turret_has_delay = true;
                    out_spawn.turret_delay = (float)value;
                }
                else if (key == "speed") {
                    double value;
                    if (!parse_number(value)) {
                        out_error = "Expected numeric turret.speed";
                        return false;
                    }
                    out_spawn.turret_has_speed = true;
                    out_spawn.turret_speed = (float)value;
                }
                else {
                    if (!skip_value()) {
                        out_error = "Couldn't skip unknown turret value";
                        return false;
                    }
                }

                skip_whitespace();
                if (consume('}')) {
                    break;
                }

                if (!consume(',')) {
                    out_error = "Expected ',' or '}' in turret object";
                    return false;
                }
            }

            return true;
        }

        bool parse_vec3(glm::vec3& out_value) {
            double x, y, z;
            if (!consume('[') || !parse_number(x) || !consume(',') || !parse_number(y) || !consume(',') || !parse_number(z) || !consume(']')) {
                return false;
            }

            out_value = glm::vec3((float)x, (float)y, (float)z);
            return true;
        }

        bool parse_quat(glm::quat& out_value) {
            double w, x, y, z;
            if (!consume('[') || !parse_number(w) || !consume(',') || !parse_number(x) || !consume(',') || !parse_number(y) || !consume(',') || !parse_number(z) || !consume(']')) {
                return false;
            }

            out_value = glm::quat((float)w, (float)x, (float)y, (float)z);
            return true;
        }

        bool parse_number(double& out_value) {
            skip_whitespace();
            size_t start = index;

            if (index < text.size() && (text[index] == '-' || text[index] == '+')) {
                ++index;
            }

            bool has_digits = false;
            while (index < text.size() && std::isdigit((unsigned char)text[index])) {
                has_digits = true;
                ++index;
            }

            if (index < text.size() && text[index] == '.') {
                ++index;
                while (index < text.size() && std::isdigit((unsigned char)text[index])) {
                    has_digits = true;
                    ++index;
                }
            }

            if (index < text.size() && (text[index] == 'e' || text[index] == 'E')) {
                ++index;
                if (index < text.size() && (text[index] == '-' || text[index] == '+')) {
                    ++index;
                }
                bool has_exp_digits = false;
                while (index < text.size() && std::isdigit((unsigned char)text[index])) {
                    has_exp_digits = true;
                    ++index;
                }
                if (!has_exp_digits) {
                    return false;
                }
            }

            if (!has_digits) {
                return false;
            }

            try {
                out_value = std::stod(text.substr(start, index - start));
            }
            catch (...) {
                return false;
            }

            return true;
        }

        bool parse_string(std::string& out) {
            skip_whitespace();
            if (index >= text.size() || text[index] != '"') {
                return false;
            }

            ++index;
            out.clear();
            while (index < text.size()) {
                char c = text[index++];
                if (c == '"') {
                    return true;
                }

                if (c == '\\') {
                    if (index >= text.size()) {
                        return false;
                    }

                    char escaped = text[index++];
                    switch (escaped) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    default:
                        return false;
                    }
                    continue;
                }

                out.push_back(c);
            }

            return false;
        }

        bool skip_value() {
            skip_whitespace();
            if (index >= text.size()) {
                return false;
            }

            if (text[index] == '"') {
                std::string ignored;
                return parse_string(ignored);
            }

            if (text[index] == '{') {
                int depth = 0;
                do {
                    char c = text[index++];
                    if (c == '"') {
                        while (index < text.size()) {
                            char s = text[index++];
                            if (s == '\\' && index < text.size()) {
                                ++index;
                                continue;
                            }
                            if (s == '"') {
                                break;
                            }
                        }
                    }
                    else if (c == '{') {
                        ++depth;
                    }
                    else if (c == '}') {
                        --depth;
                    }
                } while (index < text.size() && depth > 0);

                return depth == 0;
            }

            if (text[index] == '[') {
                int depth = 0;
                do {
                    char c = text[index++];
                    if (c == '"') {
                        while (index < text.size()) {
                            char s = text[index++];
                            if (s == '\\' && index < text.size()) {
                                ++index;
                                continue;
                            }
                            if (s == '"') {
                                break;
                            }
                        }
                    }
                    else if (c == '[') {
                        ++depth;
                    }
                    else if (c == ']') {
                        --depth;
                    }
                } while (index < text.size() && depth > 0);

                return depth == 0;
            }

            if (text.compare(index, 4, "true") == 0) {
                index += 4;
                return true;
            }
            if (text.compare(index, 5, "false") == 0) {
                index += 5;
                return true;
            }
            if (text.compare(index, 4, "null") == 0) {
                index += 4;
                return true;
            }

            double ignored;
            return parse_number(ignored);
        }

        bool consume(char token) {
            skip_whitespace();
            if (index >= text.size() || text[index] != token) {
                return false;
            }
            ++index;
            return true;
        }

        void skip_whitespace() {
            while (index < text.size() && std::isspace((unsigned char)text[index])) {
                ++index;
            }
        }

        const std::string& text;
        size_t index;
    };

    bool load_level_definitions_if_needed() {
        if (g_level_definitions_loaded) {
            return !g_level_definitions.empty();
        }

        g_level_definitions_loaded = true;

        auto layout_asset_id = vpg::Config::get_string("game.level_layout", "level.layouts");
        auto layout_asset = vpg::data::Manager::load<vpg::data::Text>(layout_asset_id);
        if (layout_asset.get_asset() == nullptr) {
            std::cerr << "MapController failed to load level layout asset '" << layout_asset_id << "'\n";
            return false;
        }

        JsonCursor cursor(layout_asset->get_content());
        std::string error;
        if (!cursor.parse_level_layout(g_level_definitions, error)) {
            std::cerr << "MapController failed to parse level layout JSON from asset '"
                      << layout_asset_id << "':\n"
                      << error << "\n";
            g_level_definitions.clear();
            return false;
        }

        return true;
    }

    const LevelDefinition* get_level_definition(int index) {
        if (!load_level_definitions_if_needed()) {
            return nullptr;
        }

        if (index < 0 || index >= (int)g_level_definitions.size()) {
            return nullptr;
        }

        return &g_level_definitions[(size_t)index];
    }

    template<typename T>
    T* get_behaviour(vpg::ecs::Entity e) {
        auto b = vpg::ecs::get_component<vpg::ecs::Behaviour>(e);
        return b != nullptr ? dynamic_cast<T*>(b->get()) : nullptr;
    }
}

bool MapController::Info::serialize(memory::Stream& stream) const {
    stream.write_ref(this->player);
    stream.write_ref(this->kill_area);

    stream.write_u32((uint32_t)this->scenes.size());
    for (const auto& entry : this->scenes) {
        if (entry.second.get_asset() == nullptr) {
            std::cerr << "MapController::Info::serialize() failed:\n"
                      << "Missing scene asset for key '" << entry.first << "'\n";
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

    this->scenes.clear();
    uint32_t scene_count = stream.read_u32();
    for (uint32_t i = 0; i < scene_count; ++i) {
        auto key = stream.read_string();
        auto asset_id = stream.read_string();
        auto scene = data::Manager::load<data::Text>(asset_id);
        if (scene.get_asset() == nullptr) {
            std::cerr << "MapController::Info::deserialize() failed:\n"
                      << "No scene asset found for key '" << key << "' and asset id '" << asset_id << "'\n";
            return false;
        }

        this->scenes[key] = scene;
    }

    return !stream.failed();
}

MapController::MapController(vpg::ecs::Entity entity, const Info& info) {
    this->scenes = info.scenes;

    this->kill_area = info.kill_area;
    this->entry = this->spawn_scene("entry");
    this->exit = this->spawn_scene("exit");

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
    this->gen_level();
}

MapController::~MapController() {
    ecs::destroy_entity(this->entry);
}

void MapController::on_kill_area_collision(const physics::Manifold& manifold) {
    (void)manifold;
    this->pending_respawn = true;
}

void MapController::on_exit_area_collision(const physics::Manifold& manifold) {
    (void)manifold;
    this->pending_next_level = true;
}

void MapController::update(float dt) {
    (void)dt;

    if (this->pending_next_level) {
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
    auto level = get_level_definition(level_num);
    if (level == nullptr || level->name.empty()) {
        return "Unknown";
    }

    return level->name.c_str();
}

data::Handle<data::Text> MapController::get_scene(const std::string& key) const {
    auto it = this->scenes.find(key);
    if (it == this->scenes.end()) {
        return data::Handle<data::Text>(nullptr);
    }

    return it->second;
}

ecs::Entity MapController::spawn_scene(const std::string& key) {
    auto scene = this->get_scene(key);
    if (scene.get_asset() == nullptr) {
        std::cerr << "MapController::spawn_scene() failed:\n"
                  << "No scene mapped for key '" << key << "'\n";
        return ecs::NullEntity;
    }

    return Manager::instance(scene);
}

void MapController::gen_level() {
    for (auto& e : this->level) {
        Manager::destroy_instance(e);
    }
    this->level.clear();

    const auto* level_definition = get_level_definition(this->level_num);
    if (level_definition == nullptr) {
        std::cerr << "MapController::gen_level() failed:\n"
                  << "No level definition for index " << this->level_num << "\n";
        return;
    }

    std::cout << "\nLoading level " << (this->level_num + 1)
              << " (" << level_definition->name << ")\n";
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

    auto exit = ecs::get_component<ecs::Transform>(this->exit);
    if (exit == nullptr) {
        return;
    }

    for (const auto& spawn : level_definition->spawns) {
        auto e = this->spawn_scene(spawn.scene);
        if (e == ecs::NullEntity) {
            continue;
        }

        auto transform = ecs::get_component<ecs::Transform>(e);
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
        exit->set_position(level_definition->exit_position);
    }

    if (level_definition->has_player_distance) {
        player->controller->SetDistance(level_definition->player_distance);
    }

    if (level_definition->has_player_timer) {
        player->controller->timer = level_definition->player_timer;
    }

    player->controller->level = this->level_num + 1;
    this->player->controller->respawn(this->player->spawn_position);
    std::cout << "\n End loading level \n";
}








