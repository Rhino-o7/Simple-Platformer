#pragma once

#include <ecs/entity_manager.hpp>
#include <data/model.hpp>
#include <memory/stream.hpp>

#include <physics/manifold.hpp>
#include <physics/sphere.hpp>
#include <physics/aabb.hpp>

#include <event.hpp>

#include <glm/glm.hpp>

namespace vpg::physics {
    struct Collider {
        static constexpr char TypeName[] = "Collider";

        enum class Type {
            Sphere,
            AABB,
        };

        struct Info {
            Type type;
            bool is_static;
            Sphere sphere;
            AABB aabb;

            bool serialize(memory::Stream& stream) const;
            bool deserialize(memory::Stream& stream);
        };

        Collider(ecs::Entity entity, const Info& create_info);
        Collider(Collider&& rhs) noexcept = default;
        ~Collider() = default;

        Event<const Manifold&> on_collision;
        
        Type type;
        bool is_static;
        Sphere sphere;
        AABB aabb;
    };

}
