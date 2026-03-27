#pragma once

#include <ecs/behaviour.hpp>
#include <data/text.hpp>
#include <physics/collider.hpp>

using namespace vpg;

struct Bullet : public ecs::IBehaviour {
    static constexpr char TypeName[] = "Bullet";

    struct Info : public IBehaviour::Info {
        virtual bool serialize(memory::Stream& stream) const override;
        virtual bool deserialize(memory::Stream& stream) override;
    };

    Bullet(vpg::ecs::Entity entity, const Info& info);

    virtual void update(float dt) override;

    enum class Mode {
        Straight,
        Orbit,
    };

    Mode mode = Mode::Straight;
    ecs::Entity entity;
    glm::vec3 velocity;
    glm::vec3 last_pos;
    glm::vec3 center;
    float radius, speed, angle;
};

