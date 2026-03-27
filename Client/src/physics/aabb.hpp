#pragma once

#include <physics/manifold.hpp>

#include <glm/glm.hpp>

namespace vpg::physics {
    struct AABB {
        glm::vec3 min, max;
    };
}
