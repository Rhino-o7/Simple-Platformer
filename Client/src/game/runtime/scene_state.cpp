#include "scene_state.hpp"

#include <ecs/coordinator.hpp>
#include <ecs/transform.hpp>

#include <iostream>

using namespace vpg;
using namespace vpg::ecs;

bool game::runtime::SceneState::deserialize(memory::Stream& stream) {
    this->clean();

    uint32_t count = stream.read_u32();
    stream.push_ref_map();
    for (uint32_t i = 0; i < count; ++i) {
        auto entity = Coordinator::create_entity();
        this->entities.insert(entity);
        stream.add_ref_map(entity, (int64_t)i);
    }

    while (count--) {
        Entity entity = (Entity)stream.read_ref();
        uint32_t component_count = stream.read_u32();
        while (component_count--) {
            if (!Coordinator::add_component(entity, stream)) {
                this->clean();
                stream.pop_ref_map();
                return false;
            }
        }
    }

    stream.pop_ref_map();
    return true;
}

Entity game::runtime::SceneState::deserialize_tree(memory::Stream& stream) {
    uint32_t count = stream.read_u32();
    stream.push_ref_map();
    for (uint32_t i = 0; i < count; ++i) {
        stream.add_ref_map(Coordinator::create_entity(), (int64_t)i);
    }

    Entity root = NullEntity;

    for (uint32_t index = 0; index < count; ++index) {
        Entity entity = (Entity)stream.read_ref();
        uint32_t component_count = stream.read_u32();
        while (component_count--) {
            if (!Coordinator::add_component(entity, stream)) {
                std::cerr << "game::runtime::SceneState::deserialize_tree() failed:\n"
                          << "Couldn't add component to entity\n";
                for (uint32_t i = 0; i < count; ++i) {
                    Coordinator::destroy_entity((Entity)stream.ref_read_to_write(i));
                }
                stream.pop_ref_map();
                return NullEntity;
            }
        }

        auto transform = Coordinator::get_component<Transform>(entity);
        if (transform == nullptr) {
            std::cerr << "game::runtime::SceneState::deserialize_tree() failed:\n"
                      << "All entities in a tree must have transforms\n";
            for (uint32_t i = 0; i < count; ++i) {
                Coordinator::destroy_entity((Entity)stream.ref_read_to_write(i));
            }
            stream.pop_ref_map();
            return NullEntity;
        }

        if (transform->get_parent() == NullEntity) {
            if (root == NullEntity) {
                root = entity;
            }
            else if (root != entity) {
                std::cerr << "game::runtime::SceneState::deserialize_tree() failed:\n"
                          << "A tree must have only one root\n";
                for (uint32_t i = 0; i < count; ++i) {
                    Coordinator::destroy_entity((Entity)stream.ref_read_to_write(i));
                }
                stream.pop_ref_map();
                return NullEntity;
            }
        }
    }

    stream.pop_ref_map();
    return root;
}

void game::runtime::SceneState::clean() {
    while (!this->entities.empty()) {
        Coordinator::destroy_entity(*this->entities.begin());
        this->entities.erase(this->entities.begin());
    }
}


