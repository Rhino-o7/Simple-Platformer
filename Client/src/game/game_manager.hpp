#pragma once

#include <data/text.hpp>

#include "scene_manifest.hpp"
#include "runtime/scene_state.hpp"

using namespace vpg;

class Manager {
public:
    Manager() = delete;
    static bool load();
    static bool load_scene(const std::string& scene_name);

    static ecs::Entity instance(data::Handle<data::Text> scene);
    static void destroy_instance(ecs::Entity entity);

    static game::runtime::SceneState* scene;

private:
    static bool load_scene_asset(const std::string& scene_asset_id);
    static bool load_manifest_if_available();

    static game::SceneManifest scene_manifest;
    static bool scene_manifest_loaded;
};


