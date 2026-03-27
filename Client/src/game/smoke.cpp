#include "smoke.hpp"

#include <ecs/transform.hpp>

#include "PerlinNoise.hpp"
#include "game/game_manager.hpp"
#include "data/manager.hpp"

//double noise;
//float windSpeed;
unsigned int smokeSeed = 200;
//siv::PerlinNoise perlin{ smokeSeed };

bool Smoke::Info::serialize(memory::Stream& stream) const {
    stream.write_f32(this->center.x);
    stream.write_f32(this->center.y);
    stream.write_f32(this->center.z);
    stream.write_f32(this->speed);
    return !stream.failed();
}

bool Smoke::Info::deserialize(memory::Stream& stream) {
    this->center.x = stream.read_f32();
    this->center.y = stream.read_f32();
    this->center.z = stream.read_f32();
    this->speed = stream.read_f32();
    return !stream.failed();
}

Smoke::Smoke(vpg::ecs::Entity entity, const Info& info) {
    this->entity = entity;
    auto transform = ecs::get_component<ecs::Transform>(this->entity);
    this->speed = 12.0f;
    this->time = info.time;
    //this->center = info.center;

    //transform->set_position(this->center);
}

void Smoke::update(float dt) {
    this->time += dt;
    auto tf = ecs::get_component<ecs::Transform>(this->entity);
    this->velocity = tf->get_up() * this->speed;// +transform->get_right() * 15.0f; //upward speed
    tf->translate(dt * (this->velocity + (windSpeed*9, windSpeed*3, windSpeed*9)));
}








