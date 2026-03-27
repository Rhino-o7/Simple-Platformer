#pragma once
#include <ecs/behaviour.hpp>
#include <data/text.hpp>
#include <physics/collider.hpp>

using namespace vpg;

struct Firespread : public ecs::IBehaviour {
    static constexpr char TypeName[] = "Firespread";

    struct Info : public IBehaviour::Info {
        data::Handle<data::Text> smoke;
        glm::vec3 center;
        float recoil, delay, time, seed;

        virtual bool serialize(memory::Stream& stream) const override;
        virtual bool deserialize(memory::Stream& stream) override;
    };

    Firespread(vpg::ecs::Entity entity, const Info& info);
    ~Firespread();

    virtual void update(float dt) override;
    void MakeSmoke();

    void set_center(const glm::vec3& center);

    data::Handle<data::Text> smoke;
    ecs::Entity entity;
    float recoil;
    glm::vec3 center;
    float delay, time;
    double firespreadSeed;
    int frames;
    std::vector<ecs::Entity> smoke_count;
    int next_smoke;
};

//data::Handle<data::Text> turret;




