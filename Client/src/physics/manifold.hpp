#pragma once

#include <ecs/entity_manager.hpp>

#include <glm/glm.hpp>

namespace vpg::physics {
    struct Manifold {
        ecs::Entity a, b;
        float penetration;
        glm::vec3 normal;
    };
}
