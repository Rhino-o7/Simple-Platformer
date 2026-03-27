#pragma once

#include <ecs/behaviour.hpp>

#include "map_controller.hpp"
#include "player_instance.hpp"
#include "player_controller.hpp"
#include "jumper.hpp"
#include "platform.hpp"
#include "turret.hpp"
#include "bullet.hpp"
#include "firetrap.hpp"
#include "smoke.hpp"
#include "firespread.hpp"

namespace game {
    inline void register_behaviours() {
        using namespace vpg::ecs;

        Behaviour::register_type<MapController>();
        Behaviour::register_type<PlayerInstance>();
        Behaviour::register_type<PlayerController>();
        Behaviour::register_type<Jumper>();
        Behaviour::register_type<Firetrap>();
        Behaviour::register_type<Platform>();
        Behaviour::register_type<Turret>();
        Behaviour::register_type<Bullet>();
        Behaviour::register_type<Smoke>();
        Behaviour::register_type<Firespread>();
    }
}

