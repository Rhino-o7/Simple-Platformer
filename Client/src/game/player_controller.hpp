#pragma once

#include <ecs/behaviour.hpp>

#include <data/text.hpp>

#include <gl/renderer.hpp>
#include <gl/text_ui.hpp>

#include <input/mouse.hpp>
#include <input/keyboard.hpp>

#include <physics/collider.hpp>

#include <chrono>
#include <ctime>

using namespace vpg;

struct PlayerController : public ecs::IBehaviour {
    static constexpr char TypeName[] = "PlayerController";

    struct Info : public IBehaviour::Info {
        ecs::Entity torso;
        ecs::Entity lfoot, rfoot;
        ecs::Entity lhand, rhand;
        ecs::Entity feet_collider;

        virtual bool serialize(memory::Stream& stream) const override;
        virtual bool deserialize(memory::Stream& stream) override;
    };

    vpg::gl::Renderer* renderer;

    PlayerController(ecs::Entity entity, const Info& info);
    ~PlayerController();

    virtual void update(float dt) override;
    void on_feet_collision(const physics::Manifold& manifold);
    void on_body_collision(const physics::Manifold& manifold);
    void mouse_move_callback(glm::vec2 mouse);
    void mouse_scroll_callback(input::Mouse::Wheel wheel, float delta);
    void air_jump(input::Keyboard::Key space);
    void respawn(glm::vec3 position);
    void SetDistance(float dis);
    vpg::Listener mouse_move_listener;
    vpg::Listener mouse_scroll_listener;
    vpg::Listener keyboard_space_listener;
    gl::TextUI text_ui;

    ecs::Entity entity;
    ecs::Entity camera;
    ecs::Entity torso;
    ecs::Entity lfoot, rfoot;
    ecs::Entity lhand, rhand;
    ecs::Entity feet_collider;

    struct windData {
        string name;
        int x;
        int y;
        int z;
        int time;
        int entity_id;
        int speed;
    };
    windData winddata_use;
    std::vector<windData> data;
    int speedValue, dataSize;

    float camera_distance, camera_x, camera_y, disFromExit;
    bool on_floor, respawned;

    float time;
    unsigned int seed;
    int health;
    glm::vec3 torso_pos;
    glm::vec3 lfoot_pos, rfoot_pos;
    glm::vec3 lhand_pos, rhand_pos;

    glm::vec3 velocity, windPower;
    glm::vec3 floor_velocity, last_dir;

    glm::vec2 last_mouse;
    float sensitivity;

    std::chrono::seconds bill;// = 20s;
    std::chrono::steady_clock::time_point old;// = steady_clock::now();
    int timer, invuln, level;
};

