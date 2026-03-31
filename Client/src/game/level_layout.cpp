#include "level_layout.hpp"

#include <config.hpp>
#include <data/manager.hpp>
#include <data/text.hpp>

#include <cctype>
#include <iostream>

namespace {
    using game::level_layout::LevelDefinition;
    using game::level_layout::SpawnDefinition;

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

                if (spawn.prefab.empty()) {
                    out_error = "Spawn object is missing required 'prefab'";
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

                if (key == "prefab") {
                    if (!parse_string(out_spawn.prefab)) {
                        out_error = "Expected spawn prefab string";
                        return false;
                    }
                }
                else if (key == "scene") {
                    if (!parse_string(out_spawn.prefab)) {
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
