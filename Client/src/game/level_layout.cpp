#ifndef _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#endif
#ifndef _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
#define _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
#endif

#include "level_layout.hpp"
#include "json_utils.hpp"

#include <config.hpp>
#include <data/manager.hpp>
#include <data/text.hpp>

#include <iostream>

namespace {
    using game::level_layout::LevelDefinition;
    using game::level_layout::SpawnDefinition;

    std::vector<LevelDefinition> g_level_definitions;
    bool g_level_definitions_loaded = false;

    bool parse_platform(const game::json_utils::JsonValue& value, SpawnDefinition& out_spawn, std::string& out_error) {
        using game::json_utils::as_float;
        using game::json_utils::as_vec3;
        using game::json_utils::get_field;

        if (!value.IsObject()) {
            out_error = "Expected 'platform' object";
            return false;
        }

        out_spawn.has_platform = true;
        as_vec3(get_field(value, "from"), out_spawn.platform_from);
        as_vec3(get_field(value, "to"), out_spawn.platform_to);
        as_vec3(get_field(value, "center"), out_spawn.platform_center);

        if (auto speed = get_field(value, "speed"); speed != nullptr) {
            if (!as_float(speed, out_spawn.platform_speed)) {
                out_error = "Expected numeric platform.speed";
                return false;
            }
            out_spawn.platform_has_speed = true;
        }

        if (auto speed_multiplier = get_field(value, "speedMultiplier"); speed_multiplier != nullptr) {
            if (!as_float(speed_multiplier, out_spawn.platform_speed_multiplier)) {
                out_error = "Expected numeric platform.speedMultiplier";
                return false;
            }
        }

        return true;
    }

    bool parse_turret(const game::json_utils::JsonValue& value, SpawnDefinition& out_spawn, std::string& out_error) {
        using game::json_utils::as_float;
        using game::json_utils::get_field;

        if (!value.IsObject()) {
            out_error = "Expected 'turret' object";
            return false;
        }

        out_spawn.has_turret = true;
        if (auto delay = get_field(value, "delay"); delay != nullptr) {
            if (!as_float(delay, out_spawn.turret_delay)) {
                out_error = "Expected numeric turret.delay";
                return false;
            }
            out_spawn.turret_has_delay = true;
        }

        if (auto speed = get_field(value, "speed"); speed != nullptr) {
            if (!as_float(speed, out_spawn.turret_speed)) {
                out_error = "Expected numeric turret.speed";
                return false;
            }
            out_spawn.turret_has_speed = true;
        }

        return true;
    }

    bool parse_spawn(const game::json_utils::JsonValue& value, SpawnDefinition& out_spawn, std::string& out_error) {
        using game::json_utils::as_quat_wxyz;
        using game::json_utils::as_string;
        using game::json_utils::as_vec3;
        using game::json_utils::get_field;

        if (!value.IsObject()) {
            out_error = "Each spawn must be an object";
            return false;
        }

        as_string(get_field(value, "prefab"), out_spawn.prefab);
        if (out_spawn.prefab.empty()) {
            as_string(get_field(value, "scene"), out_spawn.prefab);
        }

        if (auto position = get_field(value, "position"); position != nullptr) {
            if (!as_vec3(position, out_spawn.position)) {
                out_error = "Expected position vec3";
                return false;
            }
            out_spawn.has_position = true;
        }

        if (auto rotation = get_field(value, "rotation"); rotation != nullptr) {
            if (!as_quat_wxyz(rotation, out_spawn.rotation)) {
                out_error = "Expected rotation quat [w,x,y,z]";
                return false;
            }
            out_spawn.has_rotation = true;
        }

        if (auto look_at_direction = get_field(value, "lookAtDirection"); look_at_direction != nullptr) {
            if (!as_vec3(look_at_direction, out_spawn.look_at_direction)) {
                out_error = "Expected lookAtDirection vec3";
                return false;
            }
            out_spawn.has_look_at_direction = true;
        }

        if (auto platform = get_field(value, "platform"); platform != nullptr) {
            if (!parse_platform(*platform, out_spawn, out_error)) {
                return false;
            }
        }

        if (auto turret = get_field(value, "turret"); turret != nullptr) {
            if (!parse_turret(*turret, out_spawn, out_error)) {
                return false;
            }
        }

        if (out_spawn.prefab.empty()) {
            out_error = "Spawn object is missing required 'prefab'";
            return false;
        }

        return true;
    }

    bool parse_level(const game::json_utils::JsonValue& value, size_t index, LevelDefinition& out_level, std::string& out_error) {
        using game::json_utils::as_double;
        using game::json_utils::as_float;
        using game::json_utils::as_string;
        using game::json_utils::as_vec3;
        using game::json_utils::get_field;

        if (!value.IsObject()) {
            out_error = "Each level must be an object";
            return false;
        }

        as_string(get_field(value, "name"), out_level.name);
        if (out_level.name.empty()) {
            out_level.name = "Level " + std::to_string(index + 1);
        }

        if (auto exit = get_field(value, "exit"); exit != nullptr) {
            if (!as_vec3(exit, out_level.exit_position)) {
                out_error = "Expected exit vec3";
                return false;
            }
            out_level.has_exit = true;
        }

        if (auto player_distance = get_field(value, "playerDistance"); player_distance != nullptr) {
            if (!as_float(player_distance, out_level.player_distance)) {
                out_error = "Expected numeric playerDistance";
                return false;
            }
            out_level.has_player_distance = true;
        }

        if (auto player_timer = get_field(value, "playerTimer"); player_timer != nullptr) {
            double timer;
            if (!as_double(player_timer, timer)) {
                out_error = "Expected numeric playerTimer";
                return false;
            }
            out_level.player_timer = (int)timer;
            out_level.has_player_timer = true;
        }

        if (auto spawns = get_field(value, "spawns"); spawns != nullptr) {
            if (!spawns->IsArray()) {
                out_error = "Expected 'spawns' array";
                return false;
            }

            for (const auto& spawn_value : spawns->GetArray()) {
                SpawnDefinition spawn;
                if (!parse_spawn(spawn_value, spawn, out_error)) {
                    return false;
                }
                out_level.spawns.push_back(std::move(spawn));
            }
        }

        return true;
    }

    bool parse_level_layout(const game::json_utils::JsonDocument& root, std::vector<LevelDefinition>& out_levels, std::string& out_error) {
        using game::json_utils::get_field;

        auto levels = get_field(root, "levels");
        if (levels == nullptr || !levels->IsArray() || levels->Empty()) {
            out_error = "Layout must contain a non-empty 'levels' array";
            return false;
        }

        out_levels.clear();
        out_levels.reserve(levels->Size());

        size_t index = 0;
        for (const auto& level_value : levels->GetArray()) {
            LevelDefinition level;
            if (!parse_level(level_value, index, level, out_error)) {
                return false;
            }

            out_levels.push_back(std::move(level));
            ++index;
        }

        return true;
    }

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

        game::json_utils::JsonDocument root;
        std::string error;
        if (!game::json_utils::parse_document(layout_asset->get_content(), root, error)
            || !parse_level_layout(root, g_level_definitions, error)) {
            std::cerr << "MapController failed to parse level layout JSON from asset '"
                      << layout_asset_id << "':\n"
                      << error << "\n";
            g_level_definitions.clear();
            return false;
        }

        return true;
    }
}

const game::level_layout::LevelDefinition* game::level_layout::get_level_definition(int index) {
    if (!load_level_definitions_if_needed()) {
        return nullptr;
    }

    if (index < 0 || index >= (int)g_level_definitions.size()) {
        return nullptr;
    }

    return &g_level_definitions[(size_t)index];
}
