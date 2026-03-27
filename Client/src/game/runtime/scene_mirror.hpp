#pragma once

#include "scene_state.hpp"

#ifndef flecs_STATIC
#define flecs_STATIC
#endif

#include <flecs.h>

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>

#include <data/model.hpp>

#include <unordered_map>

namespace game::runtime {
    struct RuntimeEntityRef {
        vpg::ecs::Entity value;
    };

    struct RuntimeTransformSnapshot {
        glm::mat4 global;
        glm::vec3 global_position;
        glm::quat global_rotation;
        glm::vec3 position;
        glm::quat rotation;
        glm::vec3 scale;
    };

    struct RuntimeCameraSnapshot {
        float fov;
        float z_near;
        float z_far;
        int player_health;
        int player_wind;
        int seconds;
        int level;
    };

    struct RuntimeLightSnapshot {
        int type;
        float constant;
        float linear;
        float quadratic;
        glm::vec3 ambient;
        glm::vec3 diffuse;
    };

    struct RuntimeRenderableSnapshot {
        int type;
        vpg::data::Model* model;
    };

    struct RuntimeColliderSnapshot {
        int type;
        bool is_static;
        float sphere_radius;
        glm::vec3 aabb_min;
        glm::vec3 aabb_max;
    };

    struct RuntimeCameraTag { };
    struct RuntimeLightTag { };
    struct RuntimeRenderableTag { };
    struct RuntimeColliderTag { };
    struct RuntimeBehaviourTag { };

    class SceneMirror {
    public:
        void sync(flecs::world& world, const game::runtime::SceneState* scene);
        void update_behaviours(float dt) const;
        void update_colliders(flecs::world& world) const;

    private:
        std::unordered_map<vpg::ecs::Entity, flecs::entity_t> entities;
    };
}


