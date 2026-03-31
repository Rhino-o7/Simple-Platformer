#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>
#include <vector>

namespace game::level_layout {
    struct SpawnDefinition {
        std::string prefab;

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

    const LevelDefinition* get_level_definition(int index);
}
