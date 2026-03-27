#pragma once

#include <ecs/behaviour.hpp>

#include <data/text.hpp>

#include <input/mouse.hpp>
#include <input/keyboard.hpp>

#include "player_controller.hpp"

using namespace vpg;

struct PlayerInstance : public ecs::IBehaviour {
    static constexpr char TypeName[] = "PlayerInstance";

    struct Info : public IBehaviour::Info {
        data::Handle<data::Text> scene;
        glm::vec3 position;
        ecs::Entity camera;

        virtual bool serialize(memory::Stream& stream) const override;
        virtual bool deserialize(memory::Stream& stream) override;
    };

    PlayerInstance(vpg::ecs::Entity entity, const Info& info);
    ~PlayerInstance();

    PlayerController* controller;
    ecs::Entity player;
    glm::vec3 spawn_position;
};

