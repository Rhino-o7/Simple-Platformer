#include "player_controller.hpp"
#include "jumper.hpp"
#include "platform.hpp"
#include "bullet.hpp"

#include "firetrap.hpp"
#include "firespread.hpp"

#include <input/mouse.hpp>
#include <input/keyboard.hpp>
#include <config.hpp>

#include <ecs/transform.hpp>
#include <physics/collider.hpp>

#include <glm/gtc/quaternion.hpp>
#include <glm/glm.hpp>

#include <iostream>
#include <random>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <chrono>
#include <ctime>

#include "PerlinNoise.hpp"
#include <corelib/net/websocket.hpp>
#include <gl/camera.hpp>
using namespace vpg::gl;
using namespace std::chrono;

using input::Keyboard;
using input::Mouse;
using Key = Keyboard::Key;

int air_jumps = 1;
const float jump_change_force = 30.0f;
float windSpeed = 0;// distrib(gen) * .08;
float windSpeedHold = windSpeed;
float entryPosition_x = 0.0f;
float entryPosition_z = 0.0f;
float pos_x = 0.0f;
unsigned int seed = 200;
siv::PerlinNoise perlin{ seed };

bool PlayerController::Info::serialize(memory::Stream& stream) const {
    stream.write_ref(this->torso);
    stream.write_ref(this->lfoot);
    stream.write_ref(this->rfoot);
    stream.write_ref(this->lhand);
    stream.write_ref(this->rhand);
    stream.write_ref(this->feet_collider);
    return !stream.failed();
}

bool PlayerController::Info::deserialize(memory::Stream& stream) {
    this->torso = stream.read_ref();
    this->lfoot = stream.read_ref();
    this->rfoot = stream.read_ref();
    this->lhand = stream.read_ref();
    this->rhand = stream.read_ref();
    this->feet_collider = stream.read_ref();
    return !stream.failed();
}

PlayerController::PlayerController(ecs::Entity entity, const Info& info) {
#pragma region
    std::ifstream file("src/game/my_simulation.csv");
    if (!file.is_open()) {
        file.open("CoreLib/src/game/my_simulation.csv");
    }
    if (!file.is_open()) {
        file.open("Client/src/game/my_simulation.csv");
    }
    std::string line;

    dataSize = 0;
    speedValue = 0;
    if (file.is_open()) {
        std::getline(file, line);
        while (std::getline(file, line))
        {
            std::istringstream iss(line);
            std::string word;

            std::getline(iss, word, ',');
            winddata_use.x = std::stoi(word);
            std::getline(iss, word, ',');
            winddata_use.y = std::stoi(word);
            std::getline(iss, word, ',');
            winddata_use.z = std::stoi(word);

            std::getline(iss, word, ',');
            winddata_use.time = std::stoi(word);
            std::getline(iss, word, ',');
            winddata_use.entity_id = std::stoi(word);
            std::getline(iss, word, ',');
            winddata_use.speed = std::stoi(word);

            data.push_back(winddata_use);
            dataSize++;
        }
    }
#pragma endregion //struct seed

#pragma region //parts colliders and listeners
    this->entity = entity;
    this->torso = info.torso;
    this->lfoot = info.lfoot;
    this->rfoot = info.rfoot;
    this->lhand = info.lhand;
    this->rhand = info.rhand;
    this->feet_collider = info.feet_collider;

    this->velocity = glm::vec3(0.0f, 0.0f, 0.0f);
    this->windPower = glm::vec3(windSpeed *.1f, 0.0f, 0.0f);
    this->last_mouse = glm::vec2(INFINITY, INFINITY);
    this->time = 0.0f;

    this->camera_distance = 60.0f;
    this->camera_x = glm::pi<float>() / 4.0f;
    this->camera_y = glm::pi<float>() / 4.0f;
    this->on_floor = false;
    this->respawned = false;
    this->network_spawn_synced = false;
    this->network_respawn_revision = -1;

    this->keyboard_space_listener = Keyboard::Down.add_listener(std::bind(
        &PlayerController::air_jump,
        this,
        std::placeholders::_1
    ));
    this->mouse_move_listener = Mouse::Move.add_listener(std::bind(
        &PlayerController::mouse_move_callback,
        this,
        std::placeholders::_1
    ));
    this->mouse_scroll_listener = Mouse::Scroll.add_listener(std::bind(
        &PlayerController::mouse_scroll_callback,
        this,
        std::placeholders::_1,
        std::placeholders::_2
    ));
    this->sensitivity = (float)Config::get_float("camera.sensitivity", 0.001);
    Mouse::set_mode(Mouse::Mode::Disabled);

    auto collider = ecs::get_component<physics::Collider>(this->feet_collider);
    collider->on_collision.add_listener(std::bind(
        &PlayerController::on_feet_collision,
        this,
        std::placeholders::_1
    ));

    collider = ecs::get_component<physics::Collider>(this->entity);
    collider->on_collision.add_listener(std::bind(
        &PlayerController::on_body_collision,
        this,
        std::placeholders::_1
    ));

    this->torso_pos = ecs::get_component<ecs::Transform>(this->torso)->get_position();
    this->lfoot_pos = ecs::get_component<ecs::Transform>(this->lfoot)->get_position();
    this->rfoot_pos = ecs::get_component<ecs::Transform>(this->rfoot)->get_position();
    this->lhand_pos = ecs::get_component<ecs::Transform>(this->lhand)->get_position();
    this->rhand_pos = ecs::get_component<ecs::Transform>(this->rhand)->get_position();

#pragma endregion
    this->health = 3;
    this->timer = (int)Config::get_integer("game.timer", 120);
    this->level = 1;
    this->old = steady_clock::now();
    invuln = 0;
}

PlayerController::~PlayerController() {
    Mouse::Move.remove_listener(this->mouse_move_listener);
    Mouse::Move.remove_listener(this->mouse_scroll_listener);
    Mouse::set_mode(Mouse::Mode::Normal);
}

void PlayerController::update(float dt) {
    this->respawned = false;

    auto transform = ecs::get_component<ecs::Transform>(this->entity);
    auto camera = ecs::get_component<ecs::Transform>(this->camera);
    auto torso = ecs::get_component<ecs::Transform>(this->torso);
    auto lfoot = ecs::get_component<ecs::Transform>(this->lfoot);
    auto rfoot = ecs::get_component<ecs::Transform>(this->rfoot);
    auto lhand = ecs::get_component<ecs::Transform>(this->lhand);
    auto rhand = ecs::get_component<ecs::Transform>(this->rhand);

    //windData
    double noise = perlin.octave2D_01((transform->get_position().x * 0.01) / 2, (transform->get_position().z * 0.01)/2, 4);
    windSpeed = noise;

        auto center_net = glm::vec3(0.0f, 5.0f, 0.0f);
        camera->set_position(transform->get_position() + center_net + glm::normalize(glm::vec3(
        cos(camera_x) * cos(camera_y),
        sin(camera_y),
        sin(camera_x) * cos(camera_y)
    )) * this->camera_distance);
        camera->look_at(transform->get_position() + center_net, glm::vec3(0.0f, 1.0f, 0.0f));

    if (!this->on_floor) {
        this->velocity.y -= 98.1f * dt;
    }

    glm::vec2 input = { 0.0f, 0.0f };

    float speed = 30.0f;
    const float jump_change_force = 30.0f;

    if (Keyboard::is_key_pressed(Key::LShift)) {
        speed = 50.0f;
    }

    if (Keyboard::is_key_pressed(Key::W)) {
        input.y = 1.0f;
    }
    else if (Keyboard::is_key_pressed(Key::S)) {
        input.y = -1.0f;
    }

    if (Keyboard::is_key_pressed(Key::D)) {
        input.x = 1.0f;
    }
    else if (Keyboard::is_key_pressed(Key::A)) {
        input.x = -1.0f;
    }

    auto network = corelib::net::active_client();
    const bool network_connected = network != nullptr && network->connected();

    if (network_connected) {
        if (!this->network_spawn_synced) {
            auto p = transform->get_position();
            network->send_spawn(p.x, p.y, p.z);
            this->network_spawn_synced = true;
        }

        network->send_input(
            input.x,
            input.y,
            Keyboard::is_key_pressed(Key::Space),
            Keyboard::is_key_pressed(Key::LShift)
        );

        corelib::net::PlayerState state;
        if (network->try_get_latest_state(state)) {
            this->health = state.health;
            this->level = state.level;
            this->network_respawn_revision = state.respawn_revision;
        }
    }

    if (input.x == 0.0f && input.y == 0.0f) {
        //no input
        this->last_dir = glm::vec3(0.0f, 0.0f, 0.0f);
        this->time += dt * 5.0f;
        this->time = glm::clamp(this->time, 0.0f, 1.0f);
        torso->set_position(glm::mix(torso->get_position(), this->torso_pos, time));
        lfoot->set_position(glm::mix(lfoot->get_position(), this->lfoot_pos, time));
        rfoot->set_position(glm::mix(rfoot->get_position(), this->rfoot_pos, time));
        lhand->set_position(glm::mix(lhand->get_position(), this->lhand_pos, time));
        rhand->set_position(glm::mix(rhand->get_position(), this->rhand_pos, time));
        if (this->on_floor) {
            this->velocity.x = this->floor_velocity.x;
            this->velocity.y = this->floor_velocity.y;
            this->velocity = glm::mix(this->velocity, this->floor_velocity, 30.0f * dt);
        }
    }
    else {
        //any input
        glm::vec3 desired_dir = camera->get_right() * input.x + camera->get_forward() * input.y;
        desired_dir.y = 0.0f;
        desired_dir = glm::normalize(desired_dir);
        this->last_dir = desired_dir;
        transform->look_at(transform->get_global_position() + glm::mix(
            transform->get_forward(),
            -desired_dir,
            dt * 50.0f
        ), glm::vec3(0.0f, 1.0f, 0.0f));

        if (this->on_floor) {
            //ground input
            this->time -= dt * speed;
            this->time = glm::mod(this->time + 2 * glm::pi<float>(), 2 * glm::pi<float>()) - 2 * glm::pi<float>();
            glm::vec3 desired_torso = this->torso_pos + glm::vec3(0.0f, sin(this->time), 0.0f) * 0.5f;
            glm::vec3 desired_lfoot = this->lfoot_pos + glm::vec3(0.0f, sin(this->time) + 1.0f, cos(this->time)) * 1.0f;
            glm::vec3 desired_rfoot = this->rfoot_pos + glm::vec3(0.0f, sin(this->time + glm::pi<float>()) + 1.0f, cos(this->time + glm::pi<float>())) * 1.0f;
            glm::vec3 desired_lhand = this->lhand_pos + glm::vec3(0.0f, sin(this->time + glm::pi<float>()) + 1.0f, cos(this->time + glm::pi<float>())) * 1.0f;
            glm::vec3 desired_rhand = this->rhand_pos + glm::vec3(0.0f, sin(this->time) + 1.0f, cos(this->time)) * 1.0f;
            torso->set_position(glm::mix(torso->get_position(), desired_torso, dt * 10.0f));
            lfoot->set_position(glm::mix(lfoot->get_position(), desired_lfoot, dt * 10.0f));
            rfoot->set_position(glm::mix(rfoot->get_position(), desired_rfoot, dt * 10.0f));
            lhand->set_position(glm::mix(lhand->get_position(), desired_lhand, dt * 10.0f));
            rhand->set_position(glm::mix(rhand->get_position(), desired_rhand, dt * 10.0f));

            this->velocity.y = this->floor_velocity.y;
            this->velocity = glm::mix(this->velocity, this->floor_velocity + desired_dir * speed, 25.0f * dt);
        }
        else {
            if (glm::dot(this->velocity, desired_dir) < 0.0f) {
                this->velocity += desired_dir * jump_change_force + input.x * dt;
            }
        }
    }

    if (Keyboard::is_key_pressed(Key::Space) && this->on_floor) {
        this->velocity.y += 50.0f;
        this->on_floor = false;
    }
    if (this->on_floor)
    {
        air_jumps = 2;
    }

    if (!data.empty()) {
        speedValue = std::clamp(speedValue, 0, (int)data.size() - 1);
        this->windPower = glm::vec3(data[(size_t)speedValue].speed * .001f, 0.0f, 0.0f);
    }
    else {
        this->windPower = glm::vec3(0.0f);
    }

    if (this->on_floor)
    {
        transform->translate(windPower);
    }

    this->on_floor = false;
    pos_x = transform->get_position().z;
    transform->translate(this->velocity * dt);
    auto pos_y = transform->get_position().y;
    auto pos_xx = transform->get_position().x;

    // Sync health with camera component for UI display
    auto cam = ecs::get_component<vpg::gl::Camera>(this->camera);
    if (cam != nullptr) {
        cam->player_health = this->health;
        cam->player_wind = this->windPower.x * 400;
        
        //auto seconds = std::chrono::seconds(1s);
        auto dur = steady_clock::now() - old;
        auto sec = duration_cast<seconds>(dur).count();
        if (network_connected) {
            corelib::net::PlayerState state;
            if (network->try_get_latest_state(state)) {
                cam->seconds = state.seconds;
                cam->player_wind = state.wind * 400;
            }
        }
        else {
            cam->seconds = std::max(0, timer - (int)sec);
        }

        if (!network_connected && timer < sec)
        {
            respawn(glm::vec3(0, 0, 0));
            std::cout << "\n\n\n\n\n\n TIME \n";
        }
    }
    invuln++;
}

void PlayerController::on_feet_collision(const physics::Manifold& manifold) {
    if (!this->on_floor && !this->respawned) {
        this->floor_velocity = { 0.0f, 0.0f, 0.0f };

        bool was_floor = true;
        auto behaviour = ecs::get_component<ecs::Behaviour>(manifold.a == this->entity ? manifold.b : manifold.a);
        if (behaviour != nullptr) {
            auto platform = dynamic_cast<Platform*>(behaviour->get());
            if (platform != nullptr) {
                this->floor_velocity = platform->velocity;
            }

            auto jumper = dynamic_cast<Jumper*>(behaviour->get());
            if (jumper != nullptr) {
                glm::vec3 dir = this->last_dir;
                //dir.y = 5.0f;
                this->velocity.y = -this->velocity.y;
                //this->velocity += glm::normalize(dir) * jumper->bounciness;
                was_floor = false;
            }

            auto bullet = dynamic_cast<Bullet*>(behaviour->get());
            if (bullet != nullptr) {
                this->velocity += manifold.normal * bullet->speed + bullet->velocity;
                this->velocity.y += bullet->speed;
                was_floor = false;
            }

            auto firetrap = dynamic_cast<Firetrap*>(behaviour->get());
            if (firetrap != nullptr) {
                this->velocity.y = 150.0f;
                was_floor = false;
                if (invuln > 5)
                {
                    auto network = corelib::net::active_client();
                    if (network != nullptr && network->connected()) {
                        network->send_event("DAMAGE", 1);
                        this->health = std::max(0, this->health - 1);
                    }
                    else {
                        health--;
                    }
                    invuln = 0;
                }
                if (health <= 0 && (corelib::net::active_client() == nullptr || !corelib::net::active_client()->connected()))
                    this->respawn(glm::vec3(0, 0, 0));
            }
            auto firespread = dynamic_cast<Firespread*>(behaviour->get());
            if (firespread != nullptr) {
                this->velocity.y = 150.0f;
                was_floor = false;
                if (invuln > 5)
                {
                    auto network = corelib::net::active_client();
                    if (network != nullptr && network->connected()) {
                        network->send_event("DAMAGE", 1);
                        this->health = std::max(0, this->health - 1);
                    }
                    else {
                        health--;
                    }
                    invuln = 0;
                }
                if (health <= 0 && (corelib::net::active_client() == nullptr || !corelib::net::active_client()->connected()))
                    this->respawn(glm::vec3(0, 0, 0));
            }

        }

        auto transform = ecs::get_component<ecs::Transform>(this->entity);
        transform->translate(manifold.normal * manifold.penetration);

        if (was_floor) {
            this->velocity.y = this->floor_velocity.y;
            this->on_floor = true;
        }
    }
}

void PlayerController::on_body_collision(const physics::Manifold& manifold) {
    auto transform = ecs::get_component<ecs::Transform>(this->entity);
    transform->translate(manifold.normal * manifold.penetration);

    auto behaviour = ecs::get_component<ecs::Behaviour>(manifold.a == this->entity ? manifold.b : manifold.a);
    bool was_bullet = false;
    if (behaviour != nullptr) {
        auto bullet = dynamic_cast<Bullet*>(behaviour->get());
        if (bullet != nullptr) {
            this->velocity += manifold.normal * bullet->speed;
            this->velocity.y += bullet->speed;
            was_bullet = true;
        }

        auto firetrap = dynamic_cast<Firetrap*>(behaviour->get());
        auto firespread = dynamic_cast<Firespread*>(behaviour->get());
        if (firetrap != nullptr) {
            this->velocity.y = firetrap->recoil;
            if (invuln > 5) {
                auto network = corelib::net::active_client();
                if (network != nullptr && network->connected()) {
                    network->send_event("DAMAGE", 1);
                    this->health = std::max(0, this->health - 1);
                }
                else {
                    health--;
                    if (health <= 0) {
                        this->respawn(glm::vec3(0, 0, 0));
                    }
                }
                invuln = 0;
            }
        }
        if (firespread != nullptr)
        {
            this->velocity.y = firespread->recoil;
            if (invuln > 5) {
                auto network = corelib::net::active_client();
                if (network != nullptr && network->connected()) {
                    network->send_event("DAMAGE", 1);
                    this->health = std::max(0, this->health - 1);
                }
                else {
                    health--;
                    if (health <= 0) {
                        this->respawn(glm::vec3(0, 0, 0));
                    }
                }
                invuln = 0;
            }
        }
    }

    if (!was_bullet) {
        this->velocity -= manifold.normal * glm::dot(manifold.normal, this->velocity);
    }
}

void PlayerController::mouse_move_callback(glm::vec2 mouse) {
    if (this->last_mouse.x != INFINITY) {
        auto delta = this->last_mouse - mouse;
        this->camera_x -= delta.x * this->sensitivity;
        this->camera_y -= delta.y * this->sensitivity;
        this->camera_y = glm::clamp(this->camera_y, 0.1f, glm::half_pi<float>() - 0.1f);
    }

    this->last_mouse = mouse;
}

void PlayerController::mouse_scroll_callback(Mouse::Wheel wheel, float delta) {
    this->camera_distance -= delta * 10.0f;
    this->camera_distance = glm::clamp(this->camera_distance, 15.0f, 100.0f);
}

void PlayerController::respawn(glm::vec3 position) {
    std::cout << "\n health: " << health;
    this->health = 3;
    std::cout << "\n \n current wind speed SHOULD be " << windSpeed;
    std::cout << "          disFromExit is " << disFromExit;

    auto transform = ecs::get_component<ecs::Transform>(this->entity);
    transform->set_position(position);
    entryPosition_x = position.x;
    entryPosition_z = position.z;
    this->velocity = glm::vec3(0.0f, 0.0f, 0.0f);
    this->on_floor = false;
    this->respawned = true;

    siv::PerlinNoise perlin{ seed };
    speedValue = dataSize > 0 ? rand() % dataSize : 0;
    std::cout << "\nspeedValue" << speedValue;

    old = steady_clock::now(); //initial time

    auto cam = ecs::get_component<vpg::gl::Camera>(this->camera);
    if (cam != nullptr) {
        cam->player_health = this->health;
        cam->player_wind = (this->windPower.x - 2) * 800;
        cam->level = this->level;
    }
}

void PlayerController::air_jump(Keyboard::Key space)
{
    air_jumps--;
    if (air_jumps > -1 && !this->on_floor && Keyboard::is_key_pressed(Key::Space))
    {
        this->velocity.y = 0;
        this->velocity.y += 35.0f;
    }
    //this->on_floor = false;
}

void PlayerController::SetDistance(float dis)
{
    disFromExit = dis;
}








