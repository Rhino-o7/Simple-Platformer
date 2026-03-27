#include "firespread.hpp"
#include "smoke.hpp"
#include "game_manager.hpp"

#include <ecs/transform.hpp>

#include "PerlinNoise.hpp"


bool Firespread::Info::serialize(memory::Stream& stream) const {
    stream.write_string(this->smoke.get_asset()->get_id());
    stream.write_f32(this->recoil);
    stream.write_f32(this->delay);
    stream.write_f32(this->time);
    stream.write_f32(this->center.x);
    stream.write_f32(this->center.y);
    stream.write_f32(this->center.z);
    return !stream.failed();
}

bool Firespread::Info::deserialize(memory::Stream& stream) {
    this->smoke = data::Manager::load<data::Text>(stream.read_string());
    this->recoil = stream.read_f32();
    this->delay = stream.read_f32();
    this->time = stream.read_f32();
    this->center.x = stream.read_f32();
    this->center.y = stream.read_f32();
    this->center.z = stream.read_f32();
    return !stream.failed();
}

Firespread::Firespread(vpg::ecs::Entity entity, const Info& info) {
    this->entity = entity;
    this->smoke = info.smoke;
    auto transform = ecs::Coordinator::get_component<ecs::Transform>(this->entity);

    this->center = transform->get_position();

    this->recoil = info.recoil;
    this->delay = info.delay;
    this->time = info.time;
    this->center = info.center;
    this->center.z = 5;
    this->frames = 0;

    this->smoke_count.resize(2, ecs::NullEntity);
    this->next_smoke = 0;
    //transform->set_position(this->center);
    //this->MakeSmoke();siv::PerlinNoise perlin{ smokeSeed };
}

void Firespread::set_center(const glm::vec3& center) {
    this->center = center;
    auto transform = ecs::Coordinator::get_component<ecs::Transform>(this->entity);
    //transform->set_position(this->center);
}

Firespread::~Firespread() {
    std::cout << "\n ~firetrap";
    for (int i = 0; i < this->smoke_count.size(); ++i) {
        if (this->smoke_count[i] != ecs::NullEntity) {
            //Manager::destroy_instance(this->smoke_count[i]);
            std::cout << "spam";
        }
    }
}

void Firespread::update(float dt) {
    this->time += dt;
    if (this->time > this->delay) {
        this->time -= this->delay;
        this->MakeSmoke();
    }
    //this->frames++;
    //std::cout << "update, frames at " << frames << endl;
}

void Firespread::MakeSmoke()
{
    auto transform = ecs::Coordinator::get_component<ecs::Transform>(this->entity);
    auto position = transform->get_position();

    if (this->smoke_count[this->next_smoke] != ecs::NullEntity) {
        ecs::Coordinator::destroy_entity(this->smoke_count[this->next_smoke]);
    }

    siv::PerlinNoise perlin{ 300 };
    double noiseF = perlin.octave2D_01((transform->get_position().x * 0.01) / 2, (transform->get_position().z * 0.01) / 2, 4);
    
    auto e = Manager::instance(this->smoke);
    
    auto smoke = (Smoke*)ecs::Coordinator::get_component<ecs::Behaviour>(e)->get();
    auto newTransform = ecs::Coordinator::get_component<ecs::Transform>(e);
    smoke->windSpeed = noiseF;
    newTransform->set_position(position);
    
    this->smoke_count[this->next_smoke] = e;
    this->next_smoke += 1;
    if (this->next_smoke >= (int)this->smoke_count.size()) {
        this->next_smoke = 0;
    }
    std::cout << "\nfirespread smoke: " << next_smoke;
}





