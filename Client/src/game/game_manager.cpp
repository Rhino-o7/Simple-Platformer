#include "game_manager.hpp"
#include <data/prefab_json.hpp>

#include <ecs/transform.hpp>
#include <config.hpp>

#include <iostream>
#include <vector>

namespace {
    void collect_instance_entities(vpg::ecs::Entity root, std::set<vpg::ecs::Entity>& out_entities) {
        auto world = vpg::ecs::get_world();
        std::vector<vpg::ecs::Entity> stack;
        stack.push_back(root);

        while (!stack.empty()) {
            auto current = stack.back();
            stack.pop_back();

            out_entities.insert(current);
            if (world != nullptr) {
                world->entity((flecs::entity_t)current).add<game::runtime::SceneOwned>();
            }

            auto transform = vpg::ecs::get_component<vpg::ecs::Transform>(current);
            if (transform == nullptr) {
                continue;
            }

            auto child = transform->get_child();
            while (child != vpg::ecs::NullEntity) {
                stack.push_back(child);
                auto child_transform = vpg::ecs::get_component<vpg::ecs::Transform>(child);
                if (child_transform == nullptr) {
                    break;
                }
                child = child_transform->get_next();
            }
        }
    }

    void destroy_instance_children(vpg::ecs::Entity root, std::set<vpg::ecs::Entity>* scene_entities) {
        auto transform = vpg::ecs::get_component<vpg::ecs::Transform>(root);
        if (transform == nullptr) {
            return;
        }

        auto c = transform->get_child();
        while (c != vpg::ecs::NullEntity) {
            auto e = c;
            auto child_transform = vpg::ecs::get_component<vpg::ecs::Transform>(e);
            if (child_transform == nullptr) {
                break;
            }
            c = child_transform->get_next();
            child_transform->set_parent(vpg::ecs::NullEntity);

            if (scene_entities != nullptr) {
                scene_entities->erase(e);
            }

            vpg::ecs::destroy_entity(e);
        }
    }
}

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

    collect_instance_entities(root, Manager::scene->entities);

    return root;
}

void Manager::destroy_instance(ecs::Entity entity) {
    if (Manager::scene != nullptr) {
        Manager::scene->entities.erase(entity);
    }

    if (ecs::get_component<ecs::Transform>(entity) == nullptr) {
        ecs::destroy_entity(entity);
        return;
    }

    destroy_instance_children(entity, Manager::scene != nullptr ? &Manager::scene->entities : nullptr);
    ecs::destroy_entity(entity);
}








