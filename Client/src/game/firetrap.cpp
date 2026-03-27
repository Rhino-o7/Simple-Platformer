#include "firetrap.hpp"
#include "smoke.hpp"
#include "game_manager.hpp"
#include "firespread.hpp"

#include <random>
#include <iostream>

#include <ecs/transform.hpp>

#include "PerlinNoise.hpp"


bool Firetrap::Info::serialize(memory::Stream& stream) const {
    stream.write_string(this->smoke.get_asset()->get_id());
    stream.write_string(this->firespread.get_asset()->get_id());
    stream.write_f32(this->recoil);
    stream.write_f32(this->delay);
    stream.write_f32(this->time);
    stream.write_f32(this->center.x);
    stream.write_f32(this->center.y);
    stream.write_f32(this->center.z);
    return !stream.failed();
}

bool Firetrap::Info::deserialize(memory::Stream& stream) {
    this->smoke = data::Manager::load<data::Text>(stream.read_string());
    this->firespread = data::Manager::load<data::Text>(stream.read_string());
    this->recoil = stream.read_f32();
    this->delay = stream.read_f32();
    this->time = stream.read_f32();
    this->center.x = stream.read_f32();
    this->center.y = stream.read_f32();
    this->center.z = stream.read_f32();
    return !stream.failed();
}

Firetrap::Firetrap(vpg::ecs::Entity entity, const Info& info) {
    this->entity = entity;
    this->smoke = info.smoke;
    this->firespread = info.firespread;
    auto transform = ecs::get_component<ecs::Transform>(this->entity);

    this->recoil = info.recoil;
    this->delay = info.delay;
    this->time = info.time;
    this->center = info.center;
    this->center.z = 5;
    this->frames = 0;

    this->smoke_count.resize(3, ecs::NullEntity);
    this->next_smoke = 0;

    this->spread_count.resize(3, ecs::NullEntity);
    this->next_fire = 0;

    //transform->set_position(this->center);
    //this->MakeSmoke();siv::PerlinNoise perlin{ smokeSeed };
}

void Firetrap::set_center(const glm::vec3& center) {
    //this->center = center;
    //auto transform = ecs::get_component<ecs::Transform>(this->entity);
    //transform->set_position(this->center);
}

Firetrap::~Firetrap() {
    for (int i = 0; i < this->spread_count.size(); ++i) {
        if (this->spread_count[i] != ecs::NullEntity) {
            Manager::destroy_instance(this->spread_count[i]);
        }
    }
    for (int i = 0; i < this->smoke_count.size(); ++i) {
        if (this->smoke_count[i] != ecs::NullEntity) {
            Manager::destroy_instance(this->smoke_count[i]);
        }
    }
}

void Firetrap::update(float dt) {
    this->time += dt;
    if (this->time > this->delay) {
        this->time -= this->delay;
        this->MakeSmoke();
        this->spread_fire();
    }
    //this->frames++;
    //std::cout << "update, frames at " << frames << endl;
}

void Firetrap::MakeSmoke()
{
    if (this->smoke_count[this->next_smoke] != ecs::NullEntity) {
        Manager::destroy_instance(this->smoke_count[next_smoke]);
    }

    std::cout << "   doing smoke\n";
    auto transform = ecs::get_component<ecs::Transform>(this->entity);
    if (transform == nullptr) {
        return;
    }
    auto position = transform->get_position();

    siv::PerlinNoise perlin{ 300 };
    double noiseF = perlin.octave2D_01((transform->get_position().x * 0.01) / 2, (transform->get_position().z * 0.01) / 2, 4);
    
    auto e = Manager::instance(this->smoke);
    if (e == ecs::NullEntity) {
        return;
    }

    auto behaviour = ecs::get_component<ecs::Behaviour>(e);
    auto newTransform = ecs::get_component<ecs::Transform>(e);
    auto smoke = behaviour != nullptr ? dynamic_cast<Smoke*>(behaviour->get()) : nullptr;
    if (smoke == nullptr || newTransform == nullptr) {
        return;
    }
    smoke->windSpeed = noiseF;
    newTransform->set_position(position);

    this->smoke_count[this->next_smoke] = e;
    this->next_smoke += 1;
    if (this->next_smoke >= (int)this->smoke_count.size()) {
        this->next_smoke = 0;
    }
    std::cout << "\nNextsmoke: " << this->next_smoke << "\n";
}

void Firetrap::spread_fire() {

    if (this->spread_count[this->next_fire] != ecs::NullEntity) {
        Manager::destroy_instance(this->spread_count[next_fire]);
    }

    std::cout << "\nspreadfire\n";
    auto transform = ecs::get_component<ecs::Transform>(this->entity);
    if (transform == nullptr) {
        return;
    }
    auto position = transform->get_position();
    auto e = Manager::instance(this->firespread);
    if (e == ecs::NullEntity) {
        return;
    }

    auto behaviour = ecs::get_component<ecs::Behaviour>(e);
    auto ft = behaviour != nullptr ? dynamic_cast<Firespread*>(behaviour->get()) : nullptr;
    auto tr = ecs::get_component<ecs::Transform>(e);
    if (ft == nullptr || tr == nullptr) {
        return;
    }
    //auto transform = ecs::get_component<ecs::Transform>(this->entity);
    int space = rand() % 8;
    switch (space) {
    case 0: 
        tr->set_position(position + glm::vec3(0.0, 0.0, 10.0f));
        break;
    case 1: 
        tr->set_position(position + glm::vec3(10.0, 0.0, 10.0f));
        break;
    case 2: 
        tr->set_position(position + glm::vec3(10.0, 0.0, 0.0f));
        break;
    case 3: 
        tr->set_position(position + glm::vec3(10.0, 0.0, -10.0f));
        break;
    case 4: 
        tr->set_position(position + glm::vec3(0.0, 0.0, -10.0f));
        break;
    case 5: 
        tr->set_position(position + glm::vec3(-10.0, 0.0, -10.0f));
        break;
    case 6: 
        tr->set_position(position + glm::vec3(-10.0, 0.0, 0.0f));
        break;
    case 7: 
        tr->set_position(position + glm::vec3(0.0, 0.0, 10.0f));
        break;
    }

    this->spread_count[this->next_fire] = e;
    this->next_fire += 1;
    if (this->next_fire >= (int)this->spread_count.size()) {
        this->next_fire = 0;
    }
    std::cout << "\nNextspread: " << this->next_fire << "\n";
}







