#pragma once

#ifndef flecs_STATIC
#define flecs_STATIC
#endif

#include <flecs.h>

#include <functional>
#include <string>

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

        struct FixedDelta {
            float value;
        };

        struct FixedUpdateRequest {
        };

        struct SceneLoadRequest {
            std::string scene_name;
        };

        flecs::world world;
        std::function<void(float)> fixed_update_callback;
        std::function<bool(const std::string&)> scene_loader_callback;
    };
}



