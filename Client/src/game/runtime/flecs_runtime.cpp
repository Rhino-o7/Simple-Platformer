#include "flecs_runtime.hpp"

namespace game::runtime {
    FlecsRuntime::FlecsRuntime() {
        world.component<FixedDelta>();
        world.component<FixedUpdateRequest>();
        world.component<SceneLoadRequest>();

        world.system<const SceneLoadRequest>()
            .kind(flecs::PreUpdate)
            .each([this](flecs::entity e, const SceneLoadRequest& request) {
                auto loaded = false;
                if (scene_loader_callback) {
                    loaded = scene_loader_callback(request.scene_name);
                }

                if (loaded && scene_sync_callback) {
                    scene_sync_callback(world);
                }

                e.destruct();
            });

        world.system<const FixedUpdateRequest>()
            .kind(flecs::OnUpdate)
            .each([this](const FixedUpdateRequest&) {
                if (!physics_update_callback) {
                    return;
                }

                physics_update_callback();
            });

        world.system<const FixedDelta, const FixedUpdateRequest>()
            .kind(flecs::OnUpdate)
            .each([this](const FixedDelta& dt, const FixedUpdateRequest&) {
                if (behaviour_update_callback) {
                    behaviour_update_callback(dt.value);
                }

                if (fixed_update_callback) {
                    fixed_update_callback(dt.value);
                }
            });
    }

    void FlecsRuntime::set_physics_update(std::function<void()> callback) {
        physics_update_callback = std::move(callback);
    }

    void FlecsRuntime::set_behaviour_update(std::function<void(float)> callback) {
        behaviour_update_callback = std::move(callback);
    }

    void FlecsRuntime::set_fixed_update(std::function<void(float)> callback) {
        fixed_update_callback = std::move(callback);
    }

    void FlecsRuntime::set_scene_loader(std::function<bool(const std::string&)> callback) {
        scene_loader_callback = std::move(callback);
    }

    void FlecsRuntime::set_scene_sync(std::function<void(flecs::world&)> callback) {
        scene_sync_callback = std::move(callback);
    }

    void FlecsRuntime::request_scene_load(const std::string& scene_name) {
        world.entity()
            .set<SceneLoadRequest>({ scene_name });
    }

    void FlecsRuntime::pump() {
        if (scene_sync_callback) {
            scene_sync_callback(world);
        }

        world.progress(0.0f);
    }

    void FlecsRuntime::run_fixed_update(float dt) {
        auto request = world.entity()
            .set<FixedDelta>({ dt })
            .add<FixedUpdateRequest>();

        world.progress(0.0f);
        request.destruct();
    }
}
