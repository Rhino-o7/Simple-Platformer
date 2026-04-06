#include "game_manager.hpp"
#include <data/prefab_json.hpp>

#include <ecs/transform.hpp>
#include <config.hpp>

#include <iostream>
#include <vector>

game::runtime::SceneState* Manager::scene = nullptr;
game::SceneManifest Manager::scene_manifest = {};
bool Manager::scene_manifest_loaded = false;

bool Manager::load() {
    if (Manager::load_manifest_if_available()) {
        auto start_prefab = vpg::Config::get_string("game.start_prefab", scene_manifest.entry_prefab);
        return Manager::load_scene(start_prefab);
    }

    return Manager::load_scene_asset("prefab.main");
}

bool Manager::load_scene(const std::string& scene_name) {
    if (Manager::load_manifest_if_available()) {
        std::string scene_asset_id;
        if (!Manager::scene_manifest.try_get_asset(scene_name, scene_asset_id)) {
            std::cerr << "Manager::load_scene() failed:\n"
                      << "Unknown prefab '" << scene_name << "' in prefab manifest\n";
            return false;
        }

        return Manager::load_scene_asset(scene_asset_id);
    }

    return Manager::load_scene_asset(scene_name);
}

bool Manager::load_scene_asset(const std::string& scene_asset_id) {
    auto scene_asset = data::Manager::load<data::Text>(scene_asset_id);
    if (scene_asset.get_asset() == nullptr) {
        return false;
    }

    if (Manager::scene == nullptr) {
        std::cerr << "Manager::load_scene_asset() failed:\n"
                  << "Scene state is null\n";
        return false;
    }

    Manager::scene->clean();
    return Manager::instance(scene_asset) != ecs::NullEntity;
}

bool Manager::load_manifest_if_available() {
    if (scene_manifest_loaded) {
        return true;
    }

    auto manifest_id = vpg::Config::get_string("game.prefab_manifest", "prefab.manifest");
    auto manifest = data::Manager::load<data::Text>(manifest_id);
    if (manifest.get_asset() == nullptr) {
        return false;
    }

    if (!game::SceneManifestLoader::parse(manifest->get_content(), scene_manifest)) {
        std::cerr << "Manager::load_manifest_if_available() failed:\n"
                  << "Invalid scene manifest JSON in asset '" << manifest_id << "'\n";
        return false;
    }

    scene_manifest_loaded = true;
    return true;
}

ecs::Entity Manager::instance(data::Handle<data::Text> scene) {
    std::string error;
    auto root = game::prefab_json::instantiate(scene->get_content(), error);
    if (root == ecs::NullEntity) {
        std::cerr << "Manager::instance() failed:\n"
                  << "Prefab asset '" << scene.get_asset()->get_id() << "' couldn't be instantiated from JSON:\n"
                  << error << "\n";
    }
    if (root == ecs::NullEntity || Manager::scene == nullptr) {
        return root;
    }

    std::vector<ecs::Entity> stack;
    stack.push_back(root);
    while (!stack.empty()) {
        auto current = stack.back();
        stack.pop_back();

        Manager::scene->entities.insert(current);

        auto transform = ecs::get_component<ecs::Transform>(current);
        if (transform == nullptr) {
            continue;
        }

        auto child = transform->get_child();
        while (child != ecs::NullEntity) {
            stack.push_back(child);
            auto child_transform = ecs::get_component<ecs::Transform>(child);
            if (child_transform == nullptr) {
                break;
            }
            child = child_transform->get_next();
        }
    }

    return root;
}

void Manager::destroy_instance(ecs::Entity entity) {
    if (Manager::scene != nullptr) {
        Manager::scene->entities.erase(entity);
    }

    auto transform = ecs::get_component<ecs::Transform>(entity);
    if (transform == nullptr) {
        ecs::destroy_entity(entity);
        return;
    }

    auto c = transform->get_child();
    while (c != ecs::NullEntity) {
        auto e = c;
        auto transform = ecs::get_component<ecs::Transform>(e);
        if (transform == nullptr) {
            break;
        }
        c = transform->get_next();
        transform->set_parent(ecs::NullEntity);
        if (Manager::scene != nullptr) {
            Manager::scene->entities.erase(e);
        }
        ecs::destroy_entity(e);
    }
    ecs::destroy_entity(entity);
}








