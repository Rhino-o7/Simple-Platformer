#include "map_controller.hpp"
#include "game_manager.hpp"
#include "jumper.hpp"
#include "platform.hpp"
#include "turret.hpp"
#include "firetrap.hpp"
#include "smoke.hpp"
#include "firespread.hpp"
#include <gl/renderer.hpp>

#include <ecs/transform.hpp>

#include "PerlinNoise.hpp"

#include <cmath>
#include <random>
#include <iostream>

bool MapController::Info::serialize(memory::Stream& stream) const {
    stream.write_ref(this->player);
    stream.write_ref(this->kill_area);
    stream.write_string(this->entry.get_asset()->get_id());
    stream.write_string(this->exit.get_asset()->get_id());
    stream.write_string(this->tutorial.get_asset()->get_id());
    stream.write_string(this->end_message.get_asset()->get_id());
    stream.write_string(this->platform_8.get_asset()->get_id());
    stream.write_string(this->platform_8_32.get_asset()->get_id());
    stream.write_string(this->wall_8_32.get_asset()->get_id());
    stream.write_string(this->turret.get_asset()->get_id());
    stream.write_string(this->base_8_32.get_asset()->get_id());
    stream.write_string(this->base_32.get_asset()->get_id());
    stream.write_string(this->firetrap.get_asset()->get_id());
    stream.write_string(this->grass_16.get_asset()->get_id());
    stream.write_string(this->smoke.get_asset()->get_id());
    stream.write_string(this->firespread.get_asset()->get_id());

    return !stream.failed();
}

bool MapController::Info::deserialize(memory::Stream& stream) {
    this->player = stream.read_ref();
    this->kill_area = stream.read_ref();
    this->entry = data::Manager::load<data::Text>(stream.read_string());
    this->exit = data::Manager::load<data::Text>(stream.read_string());
    this->tutorial = data::Manager::load<data::Text>(stream.read_string());
    this->end_message = data::Manager::load<data::Text>(stream.read_string());
    this->platform_8 = data::Manager::load<data::Text>(stream.read_string());
    this->platform_8_32 = data::Manager::load<data::Text>(stream.read_string());
    this->wall_8_32 = data::Manager::load<data::Text>(stream.read_string());
    this->turret = data::Manager::load<data::Text>(stream.read_string());
    this->base_32 = data::Manager::load<data::Text>(stream.read_string());
    this->base_8_32 = data::Manager::load<data::Text>(stream.read_string());
    this->firetrap = data::Manager::load<data::Text>(stream.read_string());
    this->grass_16 = data::Manager::load<data::Text>(stream.read_string());
    this->smoke = data::Manager::load<data::Text>(stream.read_string());
    this->firespread = data::Manager::load<data::Text>(stream.read_string());

    return !stream.failed();
}

MapController::MapController(vpg::ecs::Entity entity, const Info& info) {
    this->tutorial = info.tutorial;
    this->end_message = info.end_message;
    this->platform_8 = info.platform_8;
    this->platform_8_32 = info.platform_8_32;
    this->wall_8_32 = info.wall_8_32;
    this->turret = info.turret;
    this->base_32 = info.base_32;
    this->base_8_32 = info.base_8_32;
    this->firetrap = info.firetrap;
    this->grass_16 = info.grass_16;
    this->smoke = info.smoke;
    this->firespread = info.firespread;

    this->kill_area = info.kill_area;
    this->entry = Manager::instance(info.entry);
    this->exit = Manager::instance(info.exit);

    auto collider = ecs::Coordinator::get_component<physics::Collider>(info.kill_area);
    collider->on_collision.add_listener(std::bind(
        &MapController::on_kill_area_collision,
        this,
        std::placeholders::_1
    ));

    collider = ecs::Coordinator::get_component<physics::Collider>(this->exit);
    collider->on_collision.add_listener(std::bind(
        &MapController::on_exit_area_collision,
        this,
        std::placeholders::_1
    ));

    this->player = (PlayerInstance*)ecs::Coordinator::get_component<ecs::Behaviour>(info.player)->get();

    this->level_num = 0;
    this->gen_level();
}

MapController::~MapController() {
    ecs::Coordinator::destroy_entity(this->entry);
}

void MapController::on_kill_area_collision(const physics::Manifold& manifold) {
    // Respawn
    this->player->controller->respawn(this->player->spawn_position);
}

void MapController::on_exit_area_collision(const physics::Manifold& manifold) {
    // Respawn
    this->level_num += 1;
    this->gen_level();
    this->player->controller->respawn(this->player->spawn_position);
}

void MapController::gen_level() {
    for (auto& e : this->level) {
        Manager::destroy_instance(e);
    }
    this->level.clear();

    std::cout << "\nLevel number: " << level_num << "\n";
    std::random_device rd;  // a seed source for the random number engine
    std::mt19937 gen(rd()); // mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<> distrib(1, 12346);
    const siv::PerlinNoise::seed_type seed = distrib(gen);
    siv::PerlinNoise perlin{ seed };
    std::cout << "Starting to make the level \n";
    this->player->controller->seed = seed;

    auto exit = ecs::Coordinator::get_component<ecs::Transform>(this->exit);

    if (this->level_num == 0) {
        auto e = Manager::instance(this->tutorial);
        auto tutorial = ecs::Coordinator::get_component<ecs::Transform>(e);
        tutorial->set_position(glm::vec3(-60.0f, 25.0f, -50.0f));
        tutorial->set_rotation(glm::quat(1.0f, 0.0f, 1.0f, 0.0f));
        this->level.push_back(e);

        e = Manager::instance(this->grass_16);
        auto grass_16 = ecs::Coordinator::get_component<ecs::Transform>(e);
        grass_16->set_rotation(glm::angleAxis(glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f)));
        grass_16->set_position(glm::vec3(0.0f, 0.0f, -50.0f));
        this->level.push_back(e);

        e = Manager::instance(this->firetrap);
        auto fit = (Firetrap*)ecs::Coordinator::get_component<ecs::Behaviour>(e)->get();
        auto fitT = ecs::Coordinator::get_component<ecs::Transform>(e);
        fitT->set_position(glm::vec3(0.0f, 0.0f, -85.0f));
        this->level.push_back(e);

        exit->set_position(glm::vec3(0.0f, 0.0f, -120.0f));

        player->controller->SetDistance(105.0f);
        player->controller->timer = 90;
        player->controller->level = 1;
    }
    else if (this->level_num == 1) {
        auto e = Manager::instance(this->platform_8);
        auto platform = (Platform*)ecs::Coordinator::get_component<ecs::Behaviour>(e)->get();
        platform->from = glm::vec3(25.0f, 0.0f, 0.0f);
        platform->to = glm::vec3(-150.0f, 0.0f, 0.0f);
        platform->set_center(glm::vec3(0.0f, 0.0f, -40.0f));
        platform->speed = 25.0f;
        this->level.push_back(e);

        e = Manager::instance(this->grass_16);
        auto grass_16 = ecs::Coordinator::get_component<ecs::Transform>(e);
        grass_16->set_position(glm::vec3(-150.0f, 0.0f, -80.0f));
        this->level.push_back(e);

        e = Manager::instance(this->firetrap);
        auto fit = (Firetrap*)ecs::Coordinator::get_component<ecs::Behaviour>(e)->get();
        auto fitT = ecs::Coordinator::get_component<ecs::Transform>(e);
        fitT->set_position(glm::vec3(0.0f, 0.0f, -70.0f));
        this->level.push_back(e);

        e = Manager::instance(this->platform_8);
        platform = (Platform*)ecs::Coordinator::get_component<ecs::Behaviour>(e)->get();
        platform->from = glm::vec3(-95.0f, 0.0f, 0.0f);
        platform->to = glm::vec3(-150.0f, 0.0f, 0.0f);
        platform->set_center(glm::vec3(0.0f, 0.0f, -120.0f));
        this->level.push_back(e);

        e = Manager::instance(this->platform_8);
        platform = (Platform*)ecs::Coordinator::get_component<ecs::Behaviour>(e)->get();
        platform->to = glm::vec3(0.0f, 0.0f, 0.0f);
        platform->from = glm::vec3(-55.0f, 0.0f, 0.0f);
        platform->set_center(glm::vec3(0.0f, 0.0f, -120.0f));
        this->level.push_back(e);

        exit->set_position(glm::vec3(0.0f, 0.0f, -160.0f));

        player->controller->SetDistance(-200.0f);
        player->controller->timer = 100;
        player->controller->level = 2;
    }
    else if (this->level_num == 2) {
        auto e = Manager::instance(this->platform_8);
        auto platform = (Platform*)ecs::Coordinator::get_component<ecs::Behaviour>(e)->get();
        platform->from = glm::vec3(0.0f, 0.0f, -130.0f);
        platform->to = glm::vec3(0.0f, 0.0f, -30.0f);
        platform->set_center(glm::vec3(0.0f, 0.0f, 0.0f));
        this->level.push_back(e);

        e = Manager::instance(this->grass_16);
        auto grass_16 = ecs::Coordinator::get_component<ecs::Transform>(e);
        grass_16->set_position(glm::vec3(0.0f, 0.0f, -160.0f));
        this->level.push_back(e);

        e = Manager::instance(this->platform_8);
        platform = (Platform*)ecs::Coordinator::get_component<ecs::Behaviour>(e)->get();
        platform->from = glm::vec3(0.0f, -20.0f, 0.0f);
        platform->to = glm::vec3(0.0f, 100.0f, 0.0f);
        platform->set_center(glm::vec3(0.0f, 0.0f, -190.0f));
        this->level.push_back(e);

        e = Manager::instance(this->turret);
        auto turret = (Turret*)ecs::Coordinator::get_component<ecs::Behaviour>(e)->get();
        auto transform = ecs::Coordinator::get_component<ecs::Transform>(e);
        turret->delay = 0.5f;
        turret->speed = 100.0f;
        transform->set_position(glm::vec3(-40.0f, 10.0f, -60.0f));
        transform->look_at(transform->get_position() + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        this->level.push_back(e);

        e = Manager::instance(this->turret);
        turret = (Turret*)ecs::Coordinator::get_component<ecs::Behaviour>(e)->get();
        transform = ecs::Coordinator::get_component<ecs::Transform>(e);
        turret->delay = 0.5f;
        turret->speed = 100.0f;
        transform->set_position(glm::vec3(40.0f, 10.0f, -100.0f));
        transform->look_at(transform->get_position() + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        this->level.push_back(e);

        exit->set_position(glm::vec3(0.0f, 90.0f, -220.0f));

        player->controller->SetDistance(-190.0f);
        player->controller->timer = 110;
        player->controller->level = 3;
    }
    else if (this->level_num == 3) {
        auto e = Manager::instance(this->platform_8_32);
        auto platform = (Platform*)ecs::Coordinator::get_component<ecs::Behaviour>(e)->get();
        platform->from = glm::vec3(0.0f, 0.0f, -120.0f);
        platform->to = glm::vec3(0.0f, 0.0f, -60.0f);
        platform->set_center(glm::vec3(0.0f, 0.0f, 0.0f));
        this->level.push_back(e);

        e = Manager::instance(this->platform_8_32);
        platform = (Platform*)ecs::Coordinator::get_component<ecs::Behaviour>(e)->get();
        platform->from = glm::vec3(0.0f, 0.0f, -210.0f);
        platform->to = glm::vec3(0.0f, 0.0f, -270.0f);
        platform->set_center(glm::vec3(0.0f, 0.0f, 0.0f));
        this->level.push_back(e);

        e = Manager::instance(this->wall_8_32);
        platform = (Platform*)ecs::Coordinator::get_component<ecs::Behaviour>(e)->get();
        platform->from = glm::vec3(-60.0f, 0.0f, 0.0f);
        platform->to = glm::vec3(60.0f, 0.0f, 0.0f);
        platform->speed *= 2.0f;
        platform->set_center(glm::vec3(0.0f, 15.0f, -120.0f));
        this->level.push_back(e);

        e = Manager::instance(this->wall_8_32);
        platform = (Platform*)ecs::Coordinator::get_component<ecs::Behaviour>(e)->get();
        platform->from = glm::vec3(60.0f, 0.0f, 0.0f);
        platform->to = glm::vec3(-60.0f, 0.0f, 0.0f);
        platform->speed *= 2.0f;
        platform->set_center(glm::vec3(0.0f, 15.0f, -210.0f));
        this->level.push_back(e);

        e = Manager::instance(this->grass_16);
        auto grass_16 = ecs::Coordinator::get_component<ecs::Transform>(e);
        grass_16->set_position(glm::vec3(0.0f, 0.0f, -330.0f));
        this->level.push_back(e);

        e = Manager::instance(this->platform_8);
        platform = (Platform*)ecs::Coordinator::get_component<ecs::Behaviour>(e)->get();
        platform->from = glm::vec3(0.0f, -5.0f, 0.0f);
        platform->to = glm::vec3(0.0f, 5.0f, 0.0f);
        platform->speed *= 5.0f;
        platform->set_center(glm::vec3(0.0f, 0.0f, -360.0f));
        this->level.push_back(e);

        e = Manager::instance(this->grass_16);
        grass_16 = ecs::Coordinator::get_component<ecs::Transform>(e);
        grass_16->set_rotation(glm::angleAxis(glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f)));
        grass_16->set_position(glm::vec3(0.0f, 40.0f, -390.0f));
        this->level.push_back(e);

        e = Manager::instance(this->platform_8_32);
        platform = (Platform*)ecs::Coordinator::get_component<ecs::Behaviour>(e)->get();
        platform->from = glm::vec3(0.0f, 40.0f, -450.0f);
        platform->to = glm::vec3(0.0f, 40.0f, -600.0f);
        platform->set_center(glm::vec3(0.0f, 0.0f, 0.0f));
        this->level.push_back(e);

        e = Manager::instance(this->turret);
        auto turret = (Turret*)ecs::Coordinator::get_component<ecs::Behaviour>(e)->get();
        auto transform = ecs::Coordinator::get_component<ecs::Transform>(e);
        turret->delay = 0.5f;
        turret->speed = 100.0f;
        transform->set_position(glm::vec3(-40.0f, 46.0f, -460.0f));
        transform->look_at(transform->get_position() + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        this->level.push_back(e);

        e = Manager::instance(this->turret);
        turret = (Turret*)ecs::Coordinator::get_component<ecs::Behaviour>(e)->get();
        transform = ecs::Coordinator::get_component<ecs::Transform>(e);
        turret->delay = 0.5f;
        turret->speed = 100.0f;
        transform->set_position(glm::vec3(40.0f, 46.0f, -525.0f));
        transform->look_at(transform->get_position() + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        this->level.push_back(e);

        e = Manager::instance(this->turret);
        turret = (Turret*)ecs::Coordinator::get_component<ecs::Behaviour>(e)->get();
        transform = ecs::Coordinator::get_component<ecs::Transform>(e);
        turret->delay = 0.5f;
        turret->speed = 100.0f;
        transform->set_position(glm::vec3(-40.0f, 46.0f, -590.0f));
        transform->look_at(transform->get_position() + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        this->level.push_back(e);

        exit->set_position(glm::vec3(0.0f, 40.0f, -660.0f));

        player->controller->SetDistance(-660.0f);
        player->controller->level = 4;
        player->controller->timer = 120;
    }
    else if (this->level_num == 4) {
        auto e = Manager::instance(this->platform_8);
        auto platform = (Platform*)ecs::Coordinator::get_component<ecs::Behaviour>(e)->get();
        platform->from = glm::vec3(0.0f, 60.0f, -90.0f);
        platform->to = glm::vec3(0.0f, 0.0f, -30.0f);
        platform->set_center(glm::vec3(0.0f, 0.0f, 0.0f));
        platform->speed *= 2.0f;
        this->level.push_back(e);

        e = Manager::instance(this->grass_16);
        auto grass_16 = ecs::Coordinator::get_component<ecs::Transform>(e);
        grass_16->set_position(glm::vec3(0.0f, 50.0f, -130.0f));
        this->level.push_back(e);

        e = Manager::instance(this->platform_8);
        platform = (Platform*)ecs::Coordinator::get_component<ecs::Behaviour>(e)->get();
        platform->from = glm::vec3(0.0f, 50.0f, -160.0f);
        platform->to = glm::vec3(0.0f, 110.0f, -220.0f);
        platform->set_center(glm::vec3(0.0f, 0.0f, 0.0f));
        platform->speed *= 2.0f;
        this->level.push_back(e);

        e = Manager::instance(this->grass_16);
        grass_16 = ecs::Coordinator::get_component<ecs::Transform>(e);
        grass_16->set_rotation(glm::angleAxis(glm::pi<float>() / 2.0f, glm::vec3(0.0f, 1.0f, 0.0f)));
        grass_16->set_position(glm::vec3(0.0f, 100.0f, -260.0f));
        this->level.push_back(e);

        e = Manager::instance(this->turret);
        auto turret = (Turret*)ecs::Coordinator::get_component<ecs::Behaviour>(e)->get();
        auto transform = ecs::Coordinator::get_component<ecs::Transform>(e);
        turret->delay = 3.0f;
        turret->speed = 80.0f;
        transform->set_position(glm::vec3(40.0f, 110.0f, -260.0f));
        transform->look_at(transform->get_position() + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        this->level.push_back(e);

        e = Manager::instance(this->base_32);
        auto base_32 = ecs::Coordinator::get_component<ecs::Transform>(e);
        base_32->set_position(glm::vec3(-300.0f, 50.0f, -260.0f));
        this->level.push_back(e);

        e = Manager::instance(this->platform_8);
        platform = (Platform*)ecs::Coordinator::get_component<ecs::Behaviour>(e)->get();
        platform->from = glm::vec3(-300.0f, 50.0f, -310.0f);
        platform->to = glm::vec3(-300.0f, 50.0f, -370.0f);
        platform->set_center(glm::vec3(0.0f, 0.0f, 0.0f));
        this->level.push_back(e);

        e = Manager::instance(this->platform_8);
        platform = (Platform*)ecs::Coordinator::get_component<ecs::Behaviour>(e)->get();
        platform->from = glm::vec3(-300.0f, 50.0f, -445.0f);
        platform->to = glm::vec3(-300.0f, 50.0f, -385.0f);
        platform->set_center(glm::vec3(0.0f, 0.0f, 0.0f));
        this->level.push_back(e);

        exit->set_position(glm::vec3(-300.0f, 50.0f, -480.0f));

        player->controller->SetDistance(-480.0f);
        player->controller->level = 5;
    }
    else if (level_num = 5) {
        auto e = Manager::instance(this->end_message);
        auto end_msg = ecs::Coordinator::get_component<ecs::Transform>(e);
        end_msg->set_position(glm::vec3(0.0f, 25.0f, -75.0f));
        end_msg->set_rotation(glm::quat(-20.0f, 0.0f, 1.0f, 0.0f));
        this->level.push_back(e);

        exit->set_position(glm::vec3(0.0f, 0.0f, -2000.0f));

        player->controller->SetDistance(2000000.0f);
        player->controller->level = 6;
    }

    this->player->controller->respawn(this->player->spawn_position);
    std::cout << "\n End loading level \n";
}





