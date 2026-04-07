#pragma once

#ifndef flecs_STATIC
#define flecs_STATIC
#endif

#include <flecs.h>

#include <ecs/behaviour.hpp>
#include <ecs/transform.hpp>
#include <physics/collider.hpp>

#include <functional>
#include <string>
#include <vector>

namespace game::runtime {
    class FlecsRuntime {
    public:
        FlecsRuntime();

        void set_fixed_update(std::function<void(float)> callback);

        void set_scene_loader(std::function<bool(const std::string&)> callback);
        void request_scene_load(const std::string& scene_name);
        void pump();

        void run_fixed_update(float dt);

        inline flecs::world& get_world() { return world; }

    private:
        void update_colliders();
        void update_behaviours(float dt);
        void run_fixed_pipeline(float dt);

        struct RuntimeControl {
            float fixed_dt;
            bool fixed_update_active;
        };

        struct SceneLoadRequest {
            std::string scene_name;
            bool pending;
        };

        flecs::world world;
        flecs::query<vpg::ecs::Transform, vpg::physics::Collider> collider_query;
        std::vector<flecs::entity_t> fixed_tick_behaviour_entities;
        std::function<void(float)> fixed_update_callback;
        std::function<bool(const std::string&)> scene_loader_callback;
    };
}



