#pragma once

#include <ecs/entity.hpp>

#include <string>

namespace game::prefab_json {
    vpg::ecs::Entity instantiate(const std::string& json, std::string& error);
}
