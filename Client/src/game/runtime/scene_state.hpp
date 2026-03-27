#pragma once

#include <set>

#include <ecs/entity_manager.hpp>
#include <memory/stream.hpp>

namespace game::runtime {
    class SceneState {
    public:
        bool deserialize(vpg::memory::Stream& stream);
        static vpg::ecs::Entity deserialize_tree(vpg::memory::Stream& stream);
        void clean();

        std::set<vpg::ecs::Entity> entities;
    };
}


