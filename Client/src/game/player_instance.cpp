#include "player_instance.hpp"
#include "player_controller.hpp"
#include "game_manager.hpp"

#include <input/mouse.hpp>
#include <input/keyboard.hpp>
#include <config.hpp>

#include <ecs/transform.hpp>
#include <physics/collider.hpp>

#include <memory/string_stream_buffer.hpp>
#include <memory/text_stream.hpp>

#include <glm/gtc/quaternion.hpp>

using namespace vpg;

using input::Keyboard;
using input::Mouse;
using Key = Keyboard::Key;

bool PlayerInstance::Info::serialize(memory::Stream& stream) const {
    stream.write_string(this->scene.get_asset()->get_id());
    stream.write_f32(this->position.x);
    stream.write_f32(this->position.y);
    stream.write_f32(this->position.z);
    stream.write_ref(this->camera);
    return !stream.failed();
}

bool PlayerInstance::Info::deserialize(memory::Stream& stream) {
    this->scene = data::Manager::load<data::Text>(stream.read_string());
    this->position.x = stream.read_f32();
    this->position.y = stream.read_f32();
    this->position.z = stream.read_f32();
    this->camera = stream.read_ref();
    return !stream.failed() && this->scene.get_asset() != nullptr;
}

PlayerInstance::PlayerInstance(ecs::Entity entity, const Info& info) {
    this->player = Manager::instance(info.scene);
    this->spawn_position = info.position;

    if (this->player == ecs::NullEntity) {
        this->controller = nullptr;
        return;
    }

    auto transform = ecs::get_component<ecs::Transform>(this->player);
    if (transform != nullptr) {
        transform->set_position(info.position);
    }

    auto behaviour = ecs::get_component<ecs::Behaviour>(this->player);
    this->controller = behaviour != nullptr ? dynamic_cast<PlayerController*>(behaviour->get()) : nullptr;
    if (this->controller != nullptr) {
        this->controller->camera = info.camera;
    }
}

PlayerInstance::~PlayerInstance() {
    Manager::destroy_instance(this->player);
}








