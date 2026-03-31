#include "prefab_json.hpp"

#include <ecs/entity.hpp>
#include <ecs/transform.hpp>
#include <ecs/behaviour.hpp>
#include <physics/collider.hpp>
#include <gl/renderable.hpp>
#include <gl/light.hpp>
#include <gl/camera.hpp>

#include "platform.hpp"
#include "turret.hpp"
#include "firetrap.hpp"
#include "firespread.hpp"
#include "smoke.hpp"
#include "bullet.hpp"
#include "player_controller.hpp"
#include "player_instance.hpp"
#include "map_controller.hpp"

#include <data/manager.hpp>
#include <data/text.hpp>

#include <cctype>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
    struct JsonValue {
        enum class Type {
            Null,
            Bool,
            Number,
            String,
            Object,
            Array,
        } type = Type::Null;

        bool bool_value = false;
        double number_value = 0.0;
        std::string string_value;
        std::unordered_map<std::string, JsonValue> object_value;
        std::vector<JsonValue> array_value;
    };

    class JsonParser {
    public:
        explicit JsonParser(const std::string& text) : text(text), index(0) {
        }

        bool parse(JsonValue& out, std::string& error) {
            skip_ws();
            if (!parse_value(out, error)) {
                return false;
            }
            skip_ws();
            if (index != text.size()) {
                error = "Unexpected trailing characters";
                return false;
            }
            return true;
        }

    private:
        bool parse_value(JsonValue& out, std::string& error) {
            skip_ws();
            if (index >= text.size()) {
                error = "Unexpected end of input";
                return false;
            }

            char c = text[index];
            if (c == '{') return parse_object(out, error);
            if (c == '[') return parse_array(out, error);
            if (c == '"') return parse_string_value(out, error);
            if (c == 't' || c == 'f') return parse_bool(out, error);
            if (c == 'n') return parse_null(out, error);
            if (c == '-' || c == '+' || std::isdigit((unsigned char)c)) return parse_number(out, error);

            error = "Unexpected token while parsing value";
            return false;
        }

        bool parse_object(JsonValue& out, std::string& error) {
            if (!consume('{')) {
                error = "Expected '{'";
                return false;
            }

            out = {};
            out.type = JsonValue::Type::Object;

            skip_ws();
            if (consume('}')) {
                return true;
            }

            while (index < text.size()) {
                std::string key;
                if (!parse_string_raw(key)) {
                    error = "Expected object key";
                    return false;
                }

                if (!consume(':')) {
                    error = "Expected ':' after object key";
                    return false;
                }

                JsonValue value;
                if (!parse_value(value, error)) {
                    return false;
                }
                out.object_value[key] = std::move(value);

                skip_ws();
                if (consume('}')) {
                    break;
                }
                if (!consume(',')) {
                    error = "Expected ',' or '}' in object";
                    return false;
                }
            }

            return true;
        }

        bool parse_array(JsonValue& out, std::string& error) {
            if (!consume('[')) {
                error = "Expected '['";
                return false;
            }

            out = {};
            out.type = JsonValue::Type::Array;

            skip_ws();
            if (consume(']')) {
                return true;
            }

            while (index < text.size()) {
                JsonValue value;
                if (!parse_value(value, error)) {
                    return false;
                }
                out.array_value.push_back(std::move(value));

                skip_ws();
                if (consume(']')) {
                    break;
                }
                if (!consume(',')) {
                    error = "Expected ',' or ']' in array";
                    return false;
                }
            }

            return true;
        }

        bool parse_string_value(JsonValue& out, std::string& error) {
            out = {};
            out.type = JsonValue::Type::String;
            if (!parse_string_raw(out.string_value)) {
                error = "Invalid string";
                return false;
            }
            return true;
        }

        bool parse_string_raw(std::string& out) {
            skip_ws();
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

                    char e = text[index++];
                    switch (e) {
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

        bool parse_bool(JsonValue& out, std::string& error) {
            out = {};
            out.type = JsonValue::Type::Bool;
            if (text.compare(index, 4, "true") == 0) {
                index += 4;
                out.bool_value = true;
                return true;
            }
            if (text.compare(index, 5, "false") == 0) {
                index += 5;
                out.bool_value = false;
                return true;
            }
            error = "Invalid boolean";
            return false;
        }

        bool parse_null(JsonValue& out, std::string& error) {
            if (text.compare(index, 4, "null") != 0) {
                error = "Invalid null";
                return false;
            }
            index += 4;
            out = {};
            out.type = JsonValue::Type::Null;
            return true;
        }

        bool parse_number(JsonValue& out, std::string& error) {
            skip_ws();
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
                bool exp_digits = false;
                while (index < text.size() && std::isdigit((unsigned char)text[index])) {
                    exp_digits = true;
                    ++index;
                }
                if (!exp_digits) {
                    error = "Invalid exponent in number";
                    return false;
                }
            }

            if (!has_digits) {
                error = "Invalid number";
                return false;
            }

            try {
                out = {};
                out.type = JsonValue::Type::Number;
                out.number_value = std::stod(text.substr(start, index - start));
                return true;
            }
            catch (...) {
                error = "Failed to parse number";
                return false;
            }
        }

        bool consume(char c) {
            skip_ws();
            if (index >= text.size() || text[index] != c) {
                return false;
            }
            ++index;
            return true;
        }

        void skip_ws() {
            if (index == 0
                && text.size() >= 3
                && (unsigned char)text[0] == 0xEF
                && (unsigned char)text[1] == 0xBB
                && (unsigned char)text[2] == 0xBF) {
                index = 3;
            }

            while (index < text.size() && std::isspace((unsigned char)text[index])) {
                ++index;
            }
        }

        const std::string& text;
        size_t index;
    };

    const JsonValue* get_field(const JsonValue& object, const std::string& key) {
        if (object.type != JsonValue::Type::Object) {
            return nullptr;
        }
        auto it = object.object_value.find(key);
        if (it == object.object_value.end()) {
            return nullptr;
        }
        return &it->second;
    }

    bool as_string(const JsonValue* value, std::string& out) {
        if (value == nullptr || value->type != JsonValue::Type::String) {
            return false;
        }
        out = value->string_value;
        return true;
    }

    bool as_vec3(const JsonValue* value, glm::vec3& out) {
        if (value == nullptr || value->type != JsonValue::Type::Array || value->array_value.size() != 3) {
            return false;
        }
        for (int i = 0; i < 3; ++i) {
            if (value->array_value[i].type != JsonValue::Type::Number) {
                return false;
            }
        }
        out = {
            (float)value->array_value[0].number_value,
            (float)value->array_value[1].number_value,
            (float)value->array_value[2].number_value,
        };
        return true;
    }

    bool as_quat(const JsonValue* value, glm::quat& out) {
        if (value == nullptr || value->type != JsonValue::Type::Array || value->array_value.size() != 4) {
            return false;
        }
        for (int i = 0; i < 4; ++i) {
            if (value->array_value[i].type != JsonValue::Type::Number) {
                return false;
            }
        }
        const float x = (float)value->array_value[0].number_value;
        const float y = (float)value->array_value[1].number_value;
        const float z = (float)value->array_value[2].number_value;
        const float w = (float)value->array_value[3].number_value;
        out = glm::quat(w, x, y, z);
        return true;
    }

    bool as_float(const JsonValue* value, float& out) {
        if (value == nullptr || value->type != JsonValue::Type::Number) {
            return false;
        }
        out = (float)value->number_value;
        return true;
    }

    bool parse_behaviour(
        vpg::ecs::Entity entity,
        const JsonValue& behaviour,
        const std::unordered_map<std::string, vpg::ecs::Entity>& entities,
        std::string& error) {
        std::string type;
        if (!as_string(get_field(behaviour, "type"), type)) {
            error = "Behaviour component requires string field 'type'";
            return false;
        }

        if (type == Platform::TypeName) {
            Platform::Info info = {};
            if (!as_vec3(get_field(behaviour, "from"), info.from)
                || !as_vec3(get_field(behaviour, "to"), info.to)
                || !as_float(get_field(behaviour, "speed"), info.speed)) {
                error = "Platform behaviour requires from/to/speed";
                return false;
            }
            vpg::ecs::add_component<vpg::ecs::Behaviour>(entity, vpg::ecs::Behaviour::Info::create<Platform>(std::move(info)));
            return true;
        }

        if (type == Turret::TypeName) {
            Turret::Info info = {};
            std::string bullet_asset;
            if (!as_string(get_field(behaviour, "bulletAsset"), bullet_asset)
                || !as_float(get_field(behaviour, "delay"), info.delay)
                || !as_float(get_field(behaviour, "speed"), info.speed)) {
                error = "Turret behaviour requires bulletAsset/delay/speed";
                return false;
            }
            info.bullet = vpg::data::Manager::load<vpg::data::Text>(bullet_asset);
            if (info.bullet.get_asset() == nullptr) {
                error = "Invalid turret bullet asset '" + bullet_asset + "'";
                return false;
            }
            vpg::ecs::add_component<vpg::ecs::Behaviour>(entity, vpg::ecs::Behaviour::Info::create<Turret>(std::move(info)));
            return true;
        }

        if (type == Firetrap::TypeName) {
            Firetrap::Info info = {};
            std::string smoke_asset, spread_asset;
            if (!as_string(get_field(behaviour, "smokeAsset"), smoke_asset)
                || !as_string(get_field(behaviour, "firespreadAsset"), spread_asset)
                || !as_float(get_field(behaviour, "recoil"), info.recoil)
                || !as_float(get_field(behaviour, "delay"), info.delay)
                || !as_float(get_field(behaviour, "time"), info.time)
                || !as_vec3(get_field(behaviour, "center"), info.center)) {
                error = "Firetrap behaviour missing required fields";
                return false;
            }
            as_float(get_field(behaviour, "seed"), info.seed);
            info.smoke = vpg::data::Manager::load<vpg::data::Text>(smoke_asset);
            info.firespread = vpg::data::Manager::load<vpg::data::Text>(spread_asset);
            if (info.smoke.get_asset() == nullptr || info.firespread.get_asset() == nullptr) {
                error = "Invalid firetrap asset reference";
                return false;
            }
            vpg::ecs::add_component<vpg::ecs::Behaviour>(entity, vpg::ecs::Behaviour::Info::create<Firetrap>(std::move(info)));
            return true;
        }

        if (type == Firespread::TypeName) {
            Firespread::Info info = {};
            std::string smoke_asset;
            if (!as_string(get_field(behaviour, "smokeAsset"), smoke_asset)
                || !as_float(get_field(behaviour, "recoil"), info.recoil)
                || !as_float(get_field(behaviour, "delay"), info.delay)
                || !as_float(get_field(behaviour, "time"), info.time)
                || !as_vec3(get_field(behaviour, "center"), info.center)) {
                error = "Firespread behaviour missing required fields";
                return false;
            }
            as_float(get_field(behaviour, "seed"), info.seed);
            info.smoke = vpg::data::Manager::load<vpg::data::Text>(smoke_asset);
            if (info.smoke.get_asset() == nullptr) {
                error = "Invalid firespread smoke asset";
                return false;
            }
            vpg::ecs::add_component<vpg::ecs::Behaviour>(entity, vpg::ecs::Behaviour::Info::create<Firespread>(std::move(info)));
            return true;
        }

        if (type == Smoke::TypeName) {
            Smoke::Info info = {};
            if (!as_vec3(get_field(behaviour, "center"), info.center)
                || !as_float(get_field(behaviour, "speed"), info.speed)) {
                error = "Smoke behaviour requires center/speed";
                return false;
            }
            as_float(get_field(behaviour, "time"), info.time);
            vpg::ecs::add_component<vpg::ecs::Behaviour>(entity, vpg::ecs::Behaviour::Info::create<Smoke>(std::move(info)));
            return true;
        }

        if (type == Bullet::TypeName) {
            Bullet::Info info = {};
            vpg::ecs::add_component<vpg::ecs::Behaviour>(entity, vpg::ecs::Behaviour::Info::create<Bullet>(std::move(info)));
            return true;
        }

        if (type == PlayerController::TypeName) {
            PlayerController::Info info = {};
            std::string torso, lfoot, rfoot, lhand, rhand, feet;
            if (!as_string(get_field(behaviour, "torso"), torso)
                || !as_string(get_field(behaviour, "lfoot"), lfoot)
                || !as_string(get_field(behaviour, "rfoot"), rfoot)
                || !as_string(get_field(behaviour, "lhand"), lhand)
                || !as_string(get_field(behaviour, "rhand"), rhand)
                || !as_string(get_field(behaviour, "feetCollider"), feet)) {
                error = "PlayerController behaviour missing entity references";
                return false;
            }

            auto resolve = [&](const std::string& name, vpg::ecs::Entity& out_ref) -> bool {
                auto it = entities.find(name);
                if (it == entities.end()) {
                    error = "Unknown entity reference '" + name + "'";
                    return false;
                }
                out_ref = it->second;
                return true;
            };

            if (!resolve(torso, info.torso) || !resolve(lfoot, info.lfoot) || !resolve(rfoot, info.rfoot)
                || !resolve(lhand, info.lhand) || !resolve(rhand, info.rhand) || !resolve(feet, info.feet_collider)) {
                return false;
            }

            vpg::ecs::add_component<vpg::ecs::Behaviour>(entity, vpg::ecs::Behaviour::Info::create<PlayerController>(std::move(info)));
            return true;
        }

        if (type == PlayerInstance::TypeName) {
            PlayerInstance::Info info = {};
            std::string prefab_asset, camera_ref;
            const JsonValue* prefab_asset_value = get_field(behaviour, "prefabAsset");
            if (prefab_asset_value == nullptr) {
                prefab_asset_value = get_field(behaviour, "sceneAsset");
            }

            if (!as_string(prefab_asset_value, prefab_asset)
                || !as_vec3(get_field(behaviour, "position"), info.position)
                || !as_string(get_field(behaviour, "camera"), camera_ref)) {
                error = "PlayerInstance behaviour missing required fields";
                return false;
            }
            info.scene = vpg::data::Manager::load<vpg::data::Text>(prefab_asset);
            if (info.scene.get_asset() == nullptr) {
                error = "Invalid PlayerInstance prefab asset";
                return false;
            }
            auto cam_it = entities.find(camera_ref);
            if (cam_it == entities.end()) {
                error = "Unknown camera ref '" + camera_ref + "'";
                return false;
            }
            info.camera = cam_it->second;
            vpg::ecs::add_component<vpg::ecs::Behaviour>(entity, vpg::ecs::Behaviour::Info::create<PlayerInstance>(std::move(info)));
            return true;
        }

        if (type == MapController::TypeName) {
            MapController::Info info = {};
            std::string player_ref, kill_ref;
            auto prefabs_field = get_field(behaviour, "prefabs");
            if (!as_string(get_field(behaviour, "player"), player_ref)
                || !as_string(get_field(behaviour, "killArea"), kill_ref)
                || prefabs_field == nullptr
                || prefabs_field->type != JsonValue::Type::Object) {
                error = "MapController behaviour missing required fields";
                return false;
            }
            auto p_it = entities.find(player_ref);
            auto k_it = entities.find(kill_ref);
            if (p_it == entities.end() || k_it == entities.end()) {
                error = "MapController has invalid entity reference";
                return false;
            }
            info.player = p_it->second;
            info.kill_area = k_it->second;
            for (const auto& kv : prefabs_field->object_value) {
                if (kv.second.type != JsonValue::Type::String) {
                    error = "MapController prefab mapping values must be strings";
                    return false;
                }
                auto asset = vpg::data::Manager::load<vpg::data::Text>(kv.second.string_value);
                if (asset.get_asset() == nullptr) {
                    error = "Invalid MapController prefab asset '" + kv.second.string_value + "'";
                    return false;
                }
                info.prefabs[kv.first] = asset;
            }

            vpg::ecs::add_component<vpg::ecs::Behaviour>(entity, vpg::ecs::Behaviour::Info::create<MapController>(std::move(info)));
            return true;
        }

        error = "Unsupported behaviour type '" + type + "'";
        return false;
    }
}

vpg::ecs::Entity game::prefab_json::instantiate(const std::string& json, std::string& error) {
    JsonParser parser(json);
    JsonValue root;
    if (!parser.parse(root, error)) {
        return vpg::ecs::NullEntity;
    }

    auto entities_field = get_field(root, "entities");
    if (entities_field == nullptr || entities_field->type != JsonValue::Type::Array || entities_field->array_value.empty()) {
        error = "Prefab JSON must contain non-empty 'entities' array";
        return vpg::ecs::NullEntity;
    }

    std::unordered_map<std::string, vpg::ecs::Entity> entities;
    std::vector<std::pair<vpg::ecs::Entity, std::string>> pending_parents;

    for (const auto& node : entities_field->array_value) {
        if (node.type != JsonValue::Type::Object) {
            error = "Each entity entry must be an object";
            return vpg::ecs::NullEntity;
        }

        std::string id;
        if (!as_string(get_field(node, "id"), id) || id.empty()) {
            error = "Entity is missing string id";
            return vpg::ecs::NullEntity;
        }

        if (entities.find(id) == entities.end()) {
            entities[id] = vpg::ecs::create_entity();
        }
    }

    for (const auto& node : entities_field->array_value) {
        std::string id;
        as_string(get_field(node, "id"), id);
        auto entity = entities[id];

        auto components = get_field(node, "components");
        if (components == nullptr || components->type != JsonValue::Type::Object) {
            error = "Entity '" + id + "' is missing object 'components'";
            return vpg::ecs::NullEntity;
        }

        auto transform_field = get_field(*components, "transform");
        if (transform_field != nullptr) {
            vpg::ecs::Transform::Info t_info = {};
            t_info.parent = vpg::ecs::NullEntity;
            t_info.scale = { 1.0f, 1.0f, 1.0f };
            t_info.rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
            std::string parent_name;

            if (transform_field->type != JsonValue::Type::Object) {
                error = "Entity '" + id + "' transform must be object";
                return vpg::ecs::NullEntity;
            }
            as_vec3(get_field(*transform_field, "position"), t_info.position);
            as_vec3(get_field(*transform_field, "scale"), t_info.scale);
            as_quat(get_field(*transform_field, "rotation"), t_info.rotation);
            auto parent_field = get_field(*transform_field, "parent");
            if (parent_field != nullptr && parent_field->type == JsonValue::Type::String) {
                parent_name = parent_field->string_value;
            }

            vpg::ecs::add_component<vpg::ecs::Transform>(entity, t_info);
            pending_parents.emplace_back(entity, parent_name);
        }

        if (auto collider = get_field(*components, "collider"); collider != nullptr) {
            if (collider->type != JsonValue::Type::Object) {
                error = "Entity '" + id + "' collider must be object";
                return vpg::ecs::NullEntity;
            }

            vpg::physics::Collider::Info c_info = {};
            std::string body, shape;
            if (!as_string(get_field(*collider, "body"), body) || !as_string(get_field(*collider, "shape"), shape)) {
                error = "Collider requires body and shape";
                return vpg::ecs::NullEntity;
            }
            c_info.is_static = (body == "Static");
            if (shape == "AABB") {
                c_info.type = vpg::physics::Collider::Type::AABB;
                if (!as_vec3(get_field(*collider, "min"), c_info.aabb.min) || !as_vec3(get_field(*collider, "max"), c_info.aabb.max)) {
                    error = "AABB collider requires min/max";
                    return vpg::ecs::NullEntity;
                }
            }
            else if (shape == "Sphere") {
                c_info.type = vpg::physics::Collider::Type::Sphere;
                if (!as_float(get_field(*collider, "radius"), c_info.sphere.radius)) {
                    error = "Sphere collider requires radius";
                    return vpg::ecs::NullEntity;
                }
            }
            else {
                error = "Unsupported collider shape '" + shape + "'";
                return vpg::ecs::NullEntity;
            }

            vpg::ecs::add_component<vpg::physics::Collider>(entity, c_info);
        }

        if (auto renderable = get_field(*components, "renderable"); renderable != nullptr) {
            if (renderable->type != JsonValue::Type::Object) {
                error = "Entity '" + id + "' renderable must be object";
                return vpg::ecs::NullEntity;
            }

            std::string model_id;
            if (!as_string(get_field(*renderable, "model"), model_id)) {
                error = "Renderable requires model string";
                return vpg::ecs::NullEntity;
            }

            vpg::gl::Renderable::Info r_info = {};
            r_info.type = vpg::gl::Renderable::Type::Model;
            r_info.model = vpg::data::Manager::load<vpg::data::Model>(model_id);
            if (r_info.model.get_asset() == nullptr) {
                error = "Renderable model asset not found: '" + model_id + "'";
                return vpg::ecs::NullEntity;
            }
            vpg::ecs::add_component<vpg::gl::Renderable>(entity, r_info);
        }

        if (auto light = get_field(*components, "light"); light != nullptr) {
            if (light->type != JsonValue::Type::Object) {
                error = "Entity '" + id + "' light must be object";
                return vpg::ecs::NullEntity;
            }
            vpg::gl::Light::Info l_info = {};
            std::string type;
            if (!as_string(get_field(*light, "type"), type)) {
                error = "Light requires type";
                return vpg::ecs::NullEntity;
            }
            if (type == "Point") {
                l_info.type = vpg::gl::Light::Type::Point;
            }
            else if (type == "Directional") {
                l_info.type = vpg::gl::Light::Type::Directional;
            }
            else {
                error = "Unsupported light type '" + type + "'";
                return vpg::ecs::NullEntity;
            }
            as_vec3(get_field(*light, "ambient"), l_info.ambient);
            as_vec3(get_field(*light, "diffuse"), l_info.diffuse);
            as_float(get_field(*light, "constant"), l_info.constant);
            as_float(get_field(*light, "linear"), l_info.linear);
            as_float(get_field(*light, "quadratic"), l_info.quadratic);
            vpg::ecs::add_component<vpg::gl::Light>(entity, l_info);
        }

        if (auto camera = get_field(*components, "camera"); camera != nullptr) {
            if (camera->type != JsonValue::Type::Object) {
                error = "Entity '" + id + "' camera must be object";
                return vpg::ecs::NullEntity;
            }
            vpg::gl::Camera::Info c_info = {};
            as_float(get_field(*camera, "fov"), c_info.fov);
            as_float(get_field(*camera, "zNear"), c_info.z_near);
            as_float(get_field(*camera, "zFar"), c_info.z_far);
            vpg::ecs::add_component<vpg::gl::Camera>(entity, c_info);
        }

        if (auto behaviour = get_field(*components, "behaviour"); behaviour != nullptr) {
            if (behaviour->type != JsonValue::Type::Object) {
                error = "Entity '" + id + "' behaviour must be object";
                return vpg::ecs::NullEntity;
            }
            if (!parse_behaviour(entity, *behaviour, entities, error)) {
                error = "Entity '" + id + "': " + error;
                return vpg::ecs::NullEntity;
            }
        }
    }

    for (const auto& [entity, parent_name] : pending_parents) {
        if (parent_name.empty()) {
            continue;
        }

        auto it = entities.find(parent_name);
        if (it == entities.end()) {
            error = "Unknown parent entity reference '" + parent_name + "'";
            return vpg::ecs::NullEntity;
        }

        auto t = vpg::ecs::get_component<vpg::ecs::Transform>(entity);
        if (t == nullptr) {
            error = "Missing transform while applying parent";
            return vpg::ecs::NullEntity;
        }
        t->set_parent(it->second);
    }

    for (const auto& [entity, _] : pending_parents) {
        auto t = vpg::ecs::get_component<vpg::ecs::Transform>(entity);
        if (t != nullptr && t->get_parent() == vpg::ecs::NullEntity) {
            return entity;
        }
    }

    error = "Prefab has no root entity";
    return vpg::ecs::NullEntity;
}
