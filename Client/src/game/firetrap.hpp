#pragma once
#include <ecs/behaviour.hpp>
#include <data/text.hpp>
#include <physics/collider.hpp>
#include <ecs/transform.hpp>

using namespace vpg;

struct Firetrap : public ecs::IBehaviour {
    static constexpr char TypeName[] = "Firetrap";

    struct Info : public IBehaviour::Info {
        data::Handle<data::Text> smoke, firespread;
        glm::vec3 center;
        float recoil, delay, time, seed;

        virtual bool serialize(memory::Stream& stream) const override;
        virtual bool deserialize(memory::Stream& stream) override;
    };

    Firetrap(vpg::ecs::Entity entity, const Info& info);
    ~Firetrap();

    virtual void update(float dt) override;
    void MakeSmoke();
    void spread_fire();

    void set_center(const glm::vec3& center);

    data::Handle<data::Text> smoke, firespread;
    ecs::Entity entity;
    float recoil;
    glm::vec3 center, pos;
    float delay, time;
    double firetrapSeed;
    int frames;
    std::vector<ecs::Entity> smoke_count, spread_count;
    int next_smoke, next_fire;
};

//data::Handle<data::Text> turret;

