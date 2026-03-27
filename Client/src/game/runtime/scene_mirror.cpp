#include "scene_mirror.hpp"

#include <ecs/coordinator.hpp>

#include <ecs/transform.hpp>
#include <ecs/behaviour.hpp>
#include <physics/collider.hpp>
#include <gl/camera.hpp>
#include <gl/light.hpp>
#include <gl/renderable.hpp>
#include <gl/debug.hpp>

#include <vector>
#include <tuple>
#include <cmath>

namespace game::runtime {
    void SceneMirror::sync(flecs::world& world, const game::runtime::SceneState* scene) {
        if (scene == nullptr) {
            return;
        }

        world.component<RuntimeEntityRef>();
        world.component<RuntimeTransformSnapshot>();
        world.component<RuntimeCameraSnapshot>();
        world.component<RuntimeLightSnapshot>();
        world.component<RuntimeRenderableSnapshot>();
        world.component<RuntimeColliderSnapshot>();
        world.component<RuntimeCameraTag>();
        world.component<RuntimeLightTag>();
        world.component<RuntimeRenderableTag>();
        world.component<RuntimeColliderTag>();
        world.component<RuntimeBehaviourTag>();

        std::vector<vpg::ecs::Entity> removed;
        for (auto& [scene_entity, _] : entities) {
            if (scene->entities.find(scene_entity) == scene->entities.end()) {
                removed.push_back(scene_entity);
            }
        }

        for (auto scene_entity : removed) {
            world.entity(entities[scene_entity]).destruct();
            entities.erase(scene_entity);
        }

        for (auto scene_entity : scene->entities) {
            auto it = entities.find(scene_entity);
            if (it == entities.end()) {
                auto flecs_entity = world.entity();
                flecs_entity.set<RuntimeEntityRef>({ scene_entity });
                it = entities.emplace(scene_entity, flecs_entity.id()).first;
            }

            auto flecs_entity = world.entity(it->second);

            auto transform = vpg::ecs::Coordinator::get_component<vpg::ecs::Transform>(scene_entity);
            if (transform != nullptr) {
                flecs_entity.set<RuntimeTransformSnapshot>({
                    transform->get_global(),
                    transform->get_global_position(),
                    transform->get_global_rotation(),
                    transform->get_position(),
                    transform->get_rotation(),
                    transform->get_scale()
                });
            }
            else {
                flecs_entity.remove<RuntimeTransformSnapshot>();
            }

            auto camera = vpg::ecs::Coordinator::get_component<vpg::gl::Camera>(scene_entity);
            if (camera != nullptr && transform != nullptr) {
                flecs_entity.add<RuntimeCameraTag>();
                flecs_entity.set<RuntimeCameraSnapshot>({
                    camera->get_fov(),
                    camera->get_z_near(),
                    camera->get_z_far(),
                    camera->player_health,
                    camera->player_wind,
                    camera->seconds,
                    camera->level
                });
            }
            else {
                flecs_entity.remove<RuntimeCameraTag>();
                flecs_entity.remove<RuntimeCameraSnapshot>();
            }

            auto light = vpg::ecs::Coordinator::get_component<vpg::gl::Light>(scene_entity);
            if (light != nullptr && transform != nullptr) {
                flecs_entity.add<RuntimeLightTag>();
                flecs_entity.set<RuntimeLightSnapshot>({
                    (int)light->type,
                    light->constant,
                    light->linear,
                    light->quadratic,
                    light->ambient,
                    light->diffuse
                });
            }
            else {
                flecs_entity.remove<RuntimeLightTag>();
                flecs_entity.remove<RuntimeLightSnapshot>();
            }

            auto renderable = vpg::ecs::Coordinator::get_component<vpg::gl::Renderable>(scene_entity);
            if (renderable != nullptr && transform != nullptr) {
                flecs_entity.add<RuntimeRenderableTag>();
                vpg::data::Model* model = nullptr;
                if (renderable->type == vpg::gl::Renderable::Type::Model) {
                    model = renderable->model.operator->();
                }

                flecs_entity.set<RuntimeRenderableSnapshot>({
                    (int)renderable->type,
                    model
                });
            }
            else {
                flecs_entity.remove<RuntimeRenderableTag>();
                flecs_entity.remove<RuntimeRenderableSnapshot>();
            }

            if (vpg::ecs::Coordinator::get_component<vpg::physics::Collider>(scene_entity) != nullptr && transform != nullptr) {
                auto collider = vpg::ecs::Coordinator::get_component<vpg::physics::Collider>(scene_entity);
                flecs_entity.add<RuntimeColliderTag>();
                flecs_entity.set<RuntimeColliderSnapshot>({
                    (int)collider->type,
                    collider->is_static,
                    collider->sphere.radius,
                    collider->aabb.min,
                    collider->aabb.max
                });
            }
            else {
                flecs_entity.remove<RuntimeColliderTag>();
                flecs_entity.remove<RuntimeColliderSnapshot>();
            }

            if (vpg::ecs::Coordinator::get_component<vpg::ecs::Behaviour>(scene_entity) != nullptr) {
                flecs_entity.add<RuntimeBehaviourTag>();
            }
            else {
                flecs_entity.remove<RuntimeBehaviourTag>();
            }
        }
    } // Closing brace for the sync function

    void SceneMirror::update_behaviours(float dt) const {
        for (const auto& [scene_entity, _] : entities) {
            auto behaviour = vpg::ecs::Coordinator::get_component<vpg::ecs::Behaviour>(scene_entity);
            if (behaviour != nullptr) {
                behaviour->update(dt);
            }
        }
    }

    namespace {
        using SnapshotItem = std::tuple<
            vpg::ecs::Entity,
            RuntimeColliderSnapshot,
            RuntimeTransformSnapshot,
            vpg::physics::Collider*>;

        static bool sphere_vs_sphere(
            const RuntimeColliderSnapshot& a,
            const RuntimeTransformSnapshot& ta,
            const RuntimeColliderSnapshot& b,
            const RuntimeTransformSnapshot& tb,
            vpg::physics::Manifold& manifold) {
            auto n = tb.global_position - ta.global_position;
            float r = a.sphere_radius + b.sphere_radius;
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
                manifold.penetration = a.sphere_radius;
                manifold.normal = glm::vec3(1.0f, 0.0f, 0.0f);
            }

            return true;
        }

        static bool aabb_vs_aabb(
            const RuntimeColliderSnapshot& a,
            const RuntimeTransformSnapshot& ta,
            const RuntimeColliderSnapshot& b,
            const RuntimeTransformSnapshot& tb,
            vpg::physics::Manifold& manifold) {
            auto center_a = (a.aabb_min + a.aabb_max) / 2.0f;
            auto center_b = (b.aabb_min + b.aabb_max) / 2.0f;

            auto n = tb.global_position + center_b - ta.global_position - center_a;

            float a_extent = (a.aabb_max.x - a.aabb_min.x) / 2.0f;
            float b_extent = (b.aabb_max.x - b.aabb_min.x) / 2.0f;
            float x_overlap = a_extent + b_extent - fabsf(n.x);
            if (x_overlap <= 0.0f) {
                return false;
            }

            a_extent = (a.aabb_max.y - a.aabb_min.y) / 2.0f;
            b_extent = (b.aabb_max.y - b.aabb_min.y) / 2.0f;
            float y_overlap = a_extent + b_extent - fabsf(n.y);
            if (y_overlap < 0.0f) {
                return false;
            }

            a_extent = (a.aabb_max.z - a.aabb_min.z) / 2.0f;
            b_extent = (b.aabb_max.z - b.aabb_min.z) / 2.0f;
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
            const RuntimeColliderSnapshot& a,
            const RuntimeTransformSnapshot& ta,
            const RuntimeColliderSnapshot& b,
            const RuntimeTransformSnapshot& tb,
            vpg::physics::Manifold& manifold) {
            auto center_a = (a.aabb_min + a.aabb_max) / 2.0f;

            auto n = tb.global_position - ta.global_position - center_a;
            auto closest = n;

            float x_extent = (a.aabb_max.x - a.aabb_min.x) / 2.0f;
            float y_extent = (a.aabb_max.y - a.aabb_min.y) / 2.0f;
            float z_extent = (a.aabb_max.z - a.aabb_min.z) / 2.0f;

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
            auto r = b.sphere_radius;

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

    void SceneMirror::update_colliders(flecs::world& world) const {
        std::vector<SnapshotItem> colliders;

        world.each<const RuntimeEntityRef>(
            [&](flecs::entity e, const RuntimeEntityRef& entity_ref) {
                if (!e.has<RuntimeColliderTag>()) {
                    return;
                }

                auto collider = e.get<RuntimeColliderSnapshot>();
                auto transform = e.get<RuntimeTransformSnapshot>();
                if (!e.has<RuntimeColliderSnapshot>() || !e.has<RuntimeTransformSnapshot>()) {
                    return;
                }

                auto collider_component = vpg::ecs::Coordinator::get_component<vpg::physics::Collider>(entity_ref.value);
                if (collider_component == nullptr) {
                    return;
                }

                colliders.emplace_back(entity_ref.value, collider, transform, collider_component);

                switch ((vpg::physics::Collider::Type)collider.type) {
                case vpg::physics::Collider::Type::Sphere:
                    vpg::gl::Debug::draw_sphere(transform.global_position, collider.sphere_radius, { 1.0f, 1.0f, 1.0f, 1.0f });
                    break;
                case vpg::physics::Collider::Type::AABB:
                {
                    auto center = (collider.aabb_min + collider.aabb_max) / 2.0f;
                    auto scale = (collider.aabb_max - collider.aabb_min) / 2.0f;
                    vpg::gl::Debug::draw_box(transform.global_position + center, scale, { 1.0f, 1.0f, 1.0f, 1.0f });
                    break;
                }
                }
            }
        );

        vpg::physics::Manifold manifold = {};

        for (size_t i = 0; i < colliders.size(); ++i) {
            auto [entity_a, collider_a, transform_a, component_a] = colliders[i];

            for (size_t j = i + 1; j < colliders.size(); ++j) {
                auto [entity_b, collider_b, transform_b, component_b] = colliders[j];

                if (collider_a.is_static && collider_b.is_static) {
                    continue;
                }

                manifold.a = entity_a;
                manifold.b = entity_b;

                bool collided = false;
                auto type_a = (vpg::physics::Collider::Type)collider_a.type;
                auto type_b = (vpg::physics::Collider::Type)collider_b.type;
                if (type_a == vpg::physics::Collider::Type::Sphere) {
                    if (type_b == vpg::physics::Collider::Type::Sphere) {
                        collided = sphere_vs_sphere(collider_a, transform_a, collider_b, transform_b, manifold);
                    }
                    else {
                        collided = aabb_vs_sphere(collider_b, transform_b, collider_a, transform_a, manifold);
                        manifold.normal = -manifold.normal;
                    }
                }
                else {
                    if (type_b == vpg::physics::Collider::Type::Sphere) {
                        collided = aabb_vs_sphere(collider_a, transform_a, collider_b, transform_b, manifold);
                    }
                    else {
                        collided = aabb_vs_aabb(collider_a, transform_a, collider_b, transform_b, manifold);
                    }
                }

                if (collided) {
                    component_b->on_collision.fire(manifold);
                    std::swap(manifold.a, manifold.b);
                    manifold.normal = -manifold.normal;
                    component_a->on_collision.fire(manifold);
                }
            }
        }
    }
}



