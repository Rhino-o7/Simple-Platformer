#pragma once

#include <ecs/behaviour.hpp>
#include <data/text.hpp>
#include <physics/collider.hpp>

using namespace vpg;
//unsigned int notSeed;
struct Smoke : public ecs::IBehaviour {
    static constexpr char TypeName[] = "Smoke";

    struct Info : public IBehaviour::Info {
        glm::vec3 center;
        float speed;
        float time;
        //Event<const Manifold&> on_collision;

        virtual bool serialize(memory::Stream& stream) const override;
        virtual bool deserialize(memory::Stream& stream) override;
    };

    Smoke(vpg::ecs::Entity entity, const Info& info);

    virtual void update(float dt) override;

    ecs::Entity entity;
    glm::vec3 center;
    glm::vec3 velocity;
    float speed;
    float time;
    unsigned int smokeSeed;
    double noise;
    float windSpeed;
};




