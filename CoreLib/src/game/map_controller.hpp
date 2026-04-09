#pragma once

#include <ecs/behaviour.hpp>
#include <data/text.hpp>
#include <physics/collider.hpp>

#include "player_instance.hpp"
#include "player_controller.hpp"

#include <vector>
#include <unordered_map>

using namespace vpg;

struct MapController : public ecs::IBehaviour {
    static constexpr char TypeName[] = "MapController";

    struct Info : public IBehaviour::Info {
        ecs::Entity player, kill_area;
        std::unordered_map<std::string, data::Handle<data::Text>> prefabs;

        virtual bool serialize(memory::Stream& stream) const override;
        virtual bool deserialize(memory::Stream& stream) override;
    };

    MapController(vpg::ecs::Entity entity, const Info& info);
    ~MapController();

    void on_kill_area_collision(const physics::Manifold& manifold);
    void on_exit_area_collision(const physics::Manifold& manifold);
    virtual void update(float dt) override;
    void gen_level();

    static const char* get_level_name(int level_num);
    data::Handle<data::Text> get_prefab(const std::string& key) const;
    ecs::Entity spawn_prefab(const std::string& key);

    std::unordered_map<std::string, data::Handle<data::Text>> prefabs;

    ecs::Entity kill_area;
    ecs::Entity entry, exit;
    std::vector<ecs::Entity> level;
    PlayerInstance* player;
    int level_num;
    bool pending_respawn;
    bool pending_next_level;
    bool save_key_was_down;
    int network_level;
    int network_respawn_revision;
};




