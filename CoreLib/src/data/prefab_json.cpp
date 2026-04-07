#ifndef _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#endif
#ifndef _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
#define _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
#endif

#include "prefab_json.hpp"
#include "prefab_json_registry.hpp"
#include "prefab_json_parsers.hpp"

#include <ecs/transform.hpp>

#include <data/json_utils.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace {
    using JsonValue = game::json_utils::JsonValue;
    using JsonDocument = game::json_utils::JsonDocument;
    using game::json_utils::as_string;
    using game::json_utils::get_field;
    using game::json_utils::parse_document;
}

vpg::ecs::Entity game::prefab_json::instantiate(const std::string& json, std::string& error) {
    JsonDocument root;
    if (!parse_document(json, root, error)) {
        return vpg::ecs::NullEntity;
    }

    auto world = vpg::ecs::get_world();
    if (world == nullptr) {
        error = "Flecs world not bound";
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

    game::prefab_json_parsers::register_default_parsers();

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
            entities[id] = world->entity().id();
        }
    }

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
            auto parser = game::prefab_json_registry::find_component_parser(component_it->name.GetString());
            if (parser == nullptr) {
                continue;
            }

            if (!parser(entity, component_it->value, entities, pending_parents, error)) {
                error = "Entity '" + id + "' " + error;
                return vpg::ecs::NullEntity;
            }
        }
    }

    auto root_entity = vpg::ecs::NullEntity;
    for (const auto& [entity, parent_name] : pending_parents) {
        if (parent_name.empty()) {
            if (root_entity == vpg::ecs::NullEntity) {
                root_entity = entity;
            }
            continue;
        }

        auto it = entities.find(parent_name);
        if (it == entities.end()) {
            error = "Unknown parent entity reference '" + parent_name + "'";
            return vpg::ecs::NullEntity;
        }

        auto t = world->entity(entity).try_get_mut<vpg::ecs::Transform>();
        if (t == nullptr) {
            error = "Missing transform while applying parent";
            return vpg::ecs::NullEntity;
        }
        t->set_parent(it->second);
    }

    if (root_entity != vpg::ecs::NullEntity) {
        return root_entity;
    }

    error = "Prefab has no root entity";
    return vpg::ecs::NullEntity;
}
