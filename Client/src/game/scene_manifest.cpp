#include "scene_manifest.hpp"

#include <cctype>

namespace {
    class JsonCursor {
    public:
        explicit JsonCursor(const std::string& text) : text(text), index(0) {
        }

        bool parse_manifest(game::SceneManifest& out_manifest) {
            skip_whitespace();
            if (!consume('{')) {
                return false;
            }

            bool has_entry_scene = false;
            bool has_scenes = false;

            skip_whitespace();
            if (peek() == '}') {
                ++index;
                return false;
            }

            while (index < text.size()) {
                std::string key;
                if (!parse_string(key)) {
                    return false;
                }

                skip_whitespace();
                if (!consume(':')) {
                    return false;
                }

                if (key == "entryScene") {
                    if (!parse_string(out_manifest.entry_scene)) {
                        return false;
                    }
                    has_entry_scene = true;
                }
                else if (key == "scenes") {
                    if (!parse_scene_object(out_manifest)) {
                        return false;
                    }
                    has_scenes = true;
                }
                else {
                    if (!skip_value()) {
                        return false;
                    }
                }

                skip_whitespace();
                if (consume('}')) {
                    break;
                }

                if (!consume(',')) {
                    return false;
                }
            }

            if (!has_scenes || out_manifest.scenes.empty()) {
                return false;
            }

            if (!has_entry_scene) {
                out_manifest.entry_scene = out_manifest.scenes.begin()->first;
            }

            return true;
        }

    private:
        char peek() const {
            if (index >= text.size()) {
                return '\0';
            }
            return text[index];
        }

        bool consume(char token) {
            skip_whitespace();
            if (peek() != token) {
                return false;
            }
            ++index;
            return true;
        }

        void skip_whitespace() {
            while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index]))) {
                ++index;
            }
        }

        bool parse_string(std::string& out) {
            skip_whitespace();
            if (peek() != '"') {
                return false;
            }
            ++index;

            out.clear();
            while (index < text.size()) {
                auto c = text[index++];
                if (c == '"') {
                    return true;
                }

                if (c == '\\') {
                    if (index >= text.size()) {
                        return false;
                    }

                    auto escaped = text[index++];
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

        bool parse_scene_object(game::SceneManifest& out_manifest) {
            skip_whitespace();
            if (!consume('{')) {
                return false;
            }

            skip_whitespace();
            if (consume('}')) {
                return true;
            }

            while (index < text.size()) {
                std::string scene_name;
                std::string scene_asset;

                if (!parse_string(scene_name)) {
                    return false;
                }

                if (!consume(':')) {
                    return false;
                }

                if (!parse_string(scene_asset)) {
                    return false;
                }

                out_manifest.scenes[scene_name] = scene_asset;

                skip_whitespace();
                if (consume('}')) {
                    break;
                }

                if (!consume(',')) {
                    return false;
                }
            }

            return true;
        }

        bool skip_value() {
            skip_whitespace();
            auto c = peek();
            if (c == '"') {
                std::string ignored;
                return parse_string(ignored);
            }

            if (c == '{') {
                int depth = 0;
                do {
                    c = text[index++];
                    if (c == '{') {
                        ++depth;
                    }
                    else if (c == '}') {
                        --depth;
                    }
                } while (index < text.size() && depth > 0);

                return depth == 0;
            }

            if (c == '[') {
                int depth = 0;
                do {
                    c = text[index++];
                    if (c == '[') {
                        ++depth;
                    }
                    else if (c == ']') {
                        --depth;
                    }
                } while (index < text.size() && depth > 0);

                return depth == 0;
            }

            while (index < text.size()) {
                c = text[index];
                if (c == ',' || c == '}' || c == ']') {
                    return true;
                }
                ++index;
            }

            return true;
        }

        const std::string& text;
        size_t index;
    };
}

bool game::SceneManifest::try_get_asset(const std::string& scene_name, std::string& out_asset_id) const {
    auto it = scenes.find(scene_name);
    if (it == scenes.end()) {
        return false;
    }

    out_asset_id = it->second;
    return true;
}

bool game::SceneManifestLoader::parse(const std::string& json, SceneManifest& out_manifest) {
    out_manifest = {};
    JsonCursor cursor(json);
    return cursor.parse_manifest(out_manifest);
}
