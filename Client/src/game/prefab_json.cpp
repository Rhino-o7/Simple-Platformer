#ifndef _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#endif
#ifndef _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
#define _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
#endif

#include "prefab_json.hpp"
#include "json_utils.hpp"

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

#include <string>
#include <unordered_map>
#include <vector>

namespace {
    using JsonValue = game::json_utils::JsonValue;
    using JsonDocument = game::json_utils::JsonDocument;
    using game::json_utils::as_float;
    using game::json_utils::as_string;
    using game::json_utils::as_vec3;
    using game::json_utils::get_field;
    using game::json_utils::parse_document;

    bool as_quat(const JsonValue* value, glm::quat& out) {
        return game::json_utils::as_quat_xyzw(value, out);
    }

#include "prefab_json_parsers.inl"
}

vpg::ecs::Entity game::prefab_json::instantiate(const std::string& json, std::string& error) {
    JsonDocument root;
    if (!parse_document(json, root, error)) {
        return vpg::ecs::NullEntity;
    }

    auto entities_field = get_field(root, "entities");
    if (entities_field == nullptr || !entities_field->IsArray() || entities_field->Empty()) {
        error = "Prefab JSON must contain non-empty 'entities' array";
        return vpg::ecs::NullEntity;
    }

    std::unordered_map<std::string, vpg::ecs::Entity> entities;
    std::vector<std::pair<vpg::ecs::Entity, std::string>> pending_parents;
    entities.reserve(entities_field->Size());
    pending_parents.reserve(entities_field->Size());

    for (const auto& node : entities_field->GetArray()) {
        if (!node.IsObject()) {
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

    static const std::unordered_map<std::string, ComponentParser> component_parsers = {
        { "transform", parse_transform_component },
        { "collider", parse_collider_component },
        { "renderable", parse_renderable_component },
        { "light", parse_light_component },
        { "camera", parse_camera_component },
        { "behaviour", parse_behaviour_component },
    };

    for (const auto& node : entities_field->GetArray()) {
        std::string id;
        as_string(get_field(node, "id"), id);
        auto entity = entities[id];

        auto components = get_field(node, "components");
        if (components == nullptr || !components->IsObject()) {
            error = "Entity '" + id + "' is missing object 'components'";
            return vpg::ecs::NullEntity;
        }

        for (auto component_it = components->MemberBegin(); component_it != components->MemberEnd(); ++component_it) {
            auto parser_it = component_parsers.find(component_it->name.GetString());
            if (parser_it == component_parsers.end()) {
                continue;
            }

            if (!parser_it->second(entity, component_it->value, entities, pending_parents, error)) {
                error = "Entity '" + id + "' " + error;
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
