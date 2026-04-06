#include "flecs_runtime.hpp"

#include <ecs/behaviour.hpp>
#include <ecs/transform.hpp>
#include <ecs/entity.hpp>
#include <physics/collider.hpp>
#include <gl/debug.hpp>

#include <tuple>
#include <vector>
#include <cmath>

namespace {
    using ColliderItem = std::tuple<
        vpg::ecs::Entity,
        vpg::physics::Collider*,
        vpg::ecs::Transform*>;

    static bool sphere_vs_sphere(
        const vpg::physics::Collider& a,
        vpg::ecs::Transform& ta,
        const vpg::physics::Collider& b,
        vpg::ecs::Transform& tb,
        vpg::physics::Manifold& manifold) {
        auto n = tb.get_global_position() - ta.get_global_position();
        float r = a.sphere.radius + b.sphere.radius;
        auto d_squared = glm::dot(n, n);
        if (d_squared > r * r) {
            return false;
        }

        float d = sqrtf(d_squared);
        if (d != 0.0f) {
            manifold.penetration = r - d;
            manifold.normal = n / d;
        }
        else {
            manifold.penetration = a.sphere.radius;
            manifold.normal = glm::vec3(1.0f, 0.0f, 0.0f);
        }

        return true;
    }

    static bool aabb_vs_aabb(
        const vpg::physics::Collider& a,
        vpg::ecs::Transform& ta,
        const vpg::physics::Collider& b,
        vpg::ecs::Transform& tb,
        vpg::physics::Manifold& manifold) {
        auto center_a = (a.aabb.min + a.aabb.max) / 2.0f;
        auto center_b = (b.aabb.min + b.aabb.max) / 2.0f;

        auto n = tb.get_global_position() + center_b - ta.get_global_position() - center_a;

        float a_extent = (a.aabb.max.x - a.aabb.min.x) / 2.0f;
        float b_extent = (b.aabb.max.x - b.aabb.min.x) / 2.0f;
        float x_overlap = a_extent + b_extent - fabsf(n.x);
        if (x_overlap <= 0.0f) {
            return false;
        }

        a_extent = (a.aabb.max.y - a.aabb.min.y) / 2.0f;
        b_extent = (b.aabb.max.y - b.aabb.min.y) / 2.0f;
        float y_overlap = a_extent + b_extent - fabsf(n.y);
        if (y_overlap < 0.0f) {
            return false;
        }

        a_extent = (a.aabb.max.z - a.aabb.min.z) / 2.0f;
        b_extent = (b.aabb.max.z - b.aabb.min.z) / 2.0f;
        float z_overlap = a_extent + b_extent - fabsf(n.z);
        if (z_overlap < 0.0f) {
            return false;
        }

        if (x_overlap < y_overlap && x_overlap < z_overlap) {
            manifold.normal = glm::vec3(n.x < 0 ? -1.0f : 1.0f, 0.0f, 0.0f);
            manifold.penetration = x_overlap;
        }
        else if (y_overlap < x_overlap && y_overlap < z_overlap) {
            manifold.normal = glm::vec3(0.0f, n.y < 0 ? -1.0f : 1.0f, 0.0f);
            manifold.penetration = y_overlap;
        }
        else {
            manifold.normal = glm::vec3(0.0f, 0.0f, n.z < 0 ? -1.0f : 1.0f);
            manifold.penetration = z_overlap;
        }

        return true;
    }

    static bool aabb_vs_sphere(
        const vpg::physics::Collider& a,
        vpg::ecs::Transform& ta,
        const vpg::physics::Collider& b,
        vpg::ecs::Transform& tb,
        vpg::physics::Manifold& manifold) {
        auto center_a = (a.aabb.min + a.aabb.max) / 2.0f;

        auto n = tb.get_global_position() - ta.get_global_position() - center_a;
        auto closest = n;

        float x_extent = (a.aabb.max.x - a.aabb.min.x) / 2.0f;
        float y_extent = (a.aabb.max.y - a.aabb.min.y) / 2.0f;
        float z_extent = (a.aabb.max.z - a.aabb.min.z) / 2.0f;

        closest.x = glm::clamp(closest.x, -x_extent, x_extent);
        closest.y = glm::clamp(closest.y, -y_extent, y_extent);
        closest.z = glm::clamp(closest.z, -z_extent, z_extent);

        bool inside = false;
        if (n == closest) {
            inside = true;

            if (fabsf(n.x) > fabsf(n.y) && fabsf(n.x) > fabsf(n.z)) {
                closest.x = closest.x > 0.0f ? x_extent : -x_extent;
            }
            else if (fabsf(n.y) > fabsf(n.x) && fabsf(n.y) > fabsf(n.z)) {
                closest.y = closest.y > 0.0f ? y_extent : -y_extent;
            }
            else {
                closest.z = closest.z > 0.0f ? z_extent : -z_extent;
            }
        }

        auto normal = n - closest;
        auto d_squared = glm::dot(normal, normal);
        auto r = b.sphere.radius;

        if (d_squared > r * r && !inside) {
            return false;
        }

        auto d = sqrtf(d_squared);
        if (inside) {
            manifold.normal = -n;
            manifold.penetration = r - d;
        }
        else {
            manifold.normal = n;
            manifold.penetration = r - d;
        }

        return true;
    }
}

namespace game::runtime {
    FlecsRuntime::FlecsRuntime() {
        vpg::ecs::bind_world(&world);

        world.component<RuntimeControl>();
        world.component<SceneLoadRequest>();

        world.set<RuntimeControl>({ 0.0f, false });
        world.set<SceneLoadRequest>({ "", false });

        collider_query = world.query<vpg::ecs::Transform, vpg::physics::Collider>();
        fixed_tick_behaviour_entities.reserve(1024);

        auto fixed_update_collision_phase = world.entity("FixedUpdateCollision")
            .add(flecs::Phase)
            .depends_on(flecs::OnUpdate);

        auto fixed_update_behaviour_phase = world.entity("FixedUpdateBehaviour")
            .add(flecs::Phase)
            .depends_on(fixed_update_collision_phase);

        world.system<SceneLoadRequest>()
            .kind(flecs::PreUpdate)
            .each([this](SceneLoadRequest& request) {
                if (!request.pending || request.scene_name.empty()) {
                    return;
                }

                if (scene_loader_callback) {
                    scene_loader_callback(request.scene_name);
                }

                request.pending = false;
                request.scene_name.clear();
            });

        world.system<const RuntimeControl>()
            .kind(fixed_update_collision_phase)
            .each([this](const RuntimeControl& control) {
                if (control.fixed_update_active) {
                    update_colliders();
                }
            });

        world.system<const vpg::ecs::Behaviour>()
            .kind(fixed_update_behaviour_phase)
            .each([this](flecs::entity e, const vpg::ecs::Behaviour&) {
                const auto& control = world.get<RuntimeControl>();
                if (!control.fixed_update_active) {
                    return;
                }

                fixed_tick_behaviour_entities.push_back(e.id());
            });
    }

    void FlecsRuntime::set_fixed_update(std::function<void(float)> callback) {
        fixed_update_callback = std::move(callback);
    }

    void FlecsRuntime::set_scene_loader(std::function<bool(const std::string&)> callback) {
        scene_loader_callback = std::move(callback);
    }

    void FlecsRuntime::request_scene_load(const std::string& scene_name) {
        auto& request = world.get_mut<SceneLoadRequest>();
        request.scene_name = scene_name;
        request.pending = true;
        world.modified<SceneLoadRequest>();
    }

    void FlecsRuntime::pump() {
        world.progress(0.0f);
    }

    void FlecsRuntime::run_fixed_update(float dt) {
        run_fixed_pipeline(dt);
    }

    void FlecsRuntime::update_behaviours(float dt) {
        for (auto id : fixed_tick_behaviour_entities) {
            if (!world.is_alive(id)) {
                continue;
            }

            auto e = world.entity(id);
            auto behaviour = e.try_get_mut<vpg::ecs::Behaviour>();
            if (behaviour != nullptr) {
                behaviour->update(dt);
            }
        }
    }

    void FlecsRuntime::run_fixed_pipeline(float dt) {
        fixed_tick_behaviour_entities.clear();

        auto& control = world.get_mut<RuntimeControl>();
        control.fixed_dt = dt;
        control.fixed_update_active = true;
        world.modified<RuntimeControl>();

        world.progress(0.0f);

        auto& control_after = world.get_mut<RuntimeControl>();
        control_after.fixed_update_active = false;
        world.modified<RuntimeControl>();

        update_behaviours(dt);

        if (fixed_update_callback) {
            fixed_update_callback(dt);
        }
    }

    void FlecsRuntime::update_colliders() {
        std::vector<ColliderItem> colliders;

        collider_query.each(
            [&](flecs::entity e, vpg::ecs::Transform& transform, vpg::physics::Collider& collider) {
                colliders.emplace_back((vpg::ecs::Entity)e.id(), &collider, &transform);

                switch (collider.type) {
                case vpg::physics::Collider::Type::Sphere:
                    vpg::gl::Debug::draw_sphere(transform.get_global_position(), collider.sphere.radius, { 1.0f, 1.0f, 1.0f, 1.0f });
                    break;
                case vpg::physics::Collider::Type::AABB:
                {
                    auto center = (collider.aabb.min + collider.aabb.max) / 2.0f;
                    auto scale = (collider.aabb.max - collider.aabb.min) / 2.0f;
                    vpg::gl::Debug::draw_box(transform.get_global_position() + center, scale, { 1.0f, 1.0f, 1.0f, 1.0f });
                    break;
                }
                }
            }
        );

        vpg::physics::Manifold manifold = {};

        for (size_t i = 0; i < colliders.size(); ++i) {
            auto [entity_a, collider_a, transform_a] = colliders[i];

            for (size_t j = i + 1; j < colliders.size(); ++j) {
                auto [entity_b, collider_b, transform_b] = colliders[j];

                if (collider_a->is_static && collider_b->is_static) {
                    continue;
                }

                manifold.a = entity_a;
                manifold.b = entity_b;

                bool collided = false;
                auto type_a = collider_a->type;
                auto type_b = collider_b->type;
                if (type_a == vpg::physics::Collider::Type::Sphere) {
                    if (type_b == vpg::physics::Collider::Type::Sphere) {
                        collided = sphere_vs_sphere(*collider_a, *transform_a, *collider_b, *transform_b, manifold);
                    }
                    else {
                        collided = aabb_vs_sphere(*collider_b, *transform_b, *collider_a, *transform_a, manifold);
                        manifold.normal = -manifold.normal;
                    }
                }
                else {
                    if (type_b == vpg::physics::Collider::Type::Sphere) {
                        collided = aabb_vs_sphere(*collider_a, *transform_a, *collider_b, *transform_b, manifold);
                    }
                    else {
                        collided = aabb_vs_aabb(*collider_a, *transform_a, *collider_b, *transform_b, manifold);
                    }
                }

                if (collided) {
                    collider_b->on_collision.fire(manifold);
                    std::swap(manifold.a, manifold.b);
                    manifold.normal = -manifold.normal;
                    collider_a->on_collision.fire(manifold);
                }
            }
        }
    }
}



