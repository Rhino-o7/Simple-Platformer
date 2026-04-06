#pragma once

#include <data/json_utils.hpp>

#include <ecs/entity.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace game::prefab_json_registry {
    using JsonValue = game::json_utils::JsonValue;
    using EntityLookup = std::unordered_map<std::string, vpg::ecs::Entity>;
    using PendingParents = std::vector<std::pair<vpg::ecs::Entity, std::string>>;

    using BehaviourParser = bool (*)(vpg::ecs::Entity, const JsonValue&, const EntityLookup&, std::string&);
    using ComponentParser = bool (*)(vpg::ecs::Entity, const JsonValue&, const EntityLookup&, PendingParents&, std::string&);

    inline std::unordered_map<std::string, BehaviourParser>& behaviour_parsers() {
        static std::unordered_map<std::string, BehaviourParser> parsers;
        return parsers;
    }

    inline std::unordered_map<std::string, ComponentParser>& component_parsers() {
        static std::unordered_map<std::string, ComponentParser> parsers;
        return parsers;
    }

    inline void register_behaviour_parser(const std::string& type, BehaviourParser parser) {
        behaviour_parsers()[type] = parser;
    }

    inline void register_component_parser(const std::string& name, ComponentParser parser) {
        component_parsers()[name] = parser;
    }

    inline BehaviourParser find_behaviour_parser(const std::string& type) {
        auto& parsers = behaviour_parsers();
        auto it = parsers.find(type);
        return it == parsers.end() ? nullptr : it->second;
    }

    inline ComponentParser find_component_parser(const std::string& name) {
        auto& parsers = component_parsers();
        auto it = parsers.find(name);
        return it == parsers.end() ? nullptr : it->second;
    }
}
