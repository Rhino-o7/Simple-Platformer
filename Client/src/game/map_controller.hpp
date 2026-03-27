#pragma once

#include <ecs/behaviour.hpp>
#include <data/text.hpp>
#include <physics/collider.hpp>

#include "player_instance.hpp"
#include "player_controller.hpp"

#include <vector>

using namespace vpg;

struct MapController : public ecs::IBehaviour {
    static constexpr char TypeName[] = "MapController";

    struct Info : public IBehaviour::Info {
        ecs::Entity player, kill_area;
        data::Handle<data::Text> entry, exit, tutorial, end_message;
        data::Handle<data::Text> platform_8, platform_8_32, wall_8_32, turret;
        data::Handle<data::Text> base_32, base_8_32, firetrap, grass_16, smoke, firespread;

        virtual bool serialize(memory::Stream& stream) const override;
        virtual bool deserialize(memory::Stream& stream) override;
    };

    MapController(vpg::ecs::Entity entity, const Info& info);
    ~MapController();

    void on_kill_area_collision(const physics::Manifold& manifold);
    void on_exit_area_collision(const physics::Manifold& manifold);
    virtual void update(float dt) override;
    void gen_level();

    data::Handle<data::Text> tutorial, end_message;
    data::Handle<data::Text> platform_8, platform_8_32, wall_8_32, turret;
    data::Handle<data::Text> base_32, base_8_32, firetrap, grass_16, smoke, firespread;

    ecs::Entity kill_area;
    ecs::Entity entry, exit;
    std::vector<ecs::Entity> level;
    PlayerInstance* player;
    int level_num;
    bool pending_respawn;
    bool pending_next_level;
};




