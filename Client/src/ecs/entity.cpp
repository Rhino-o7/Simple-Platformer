#include <ecs/entity.hpp>
#include <ecs/transform.hpp>
#include <ecs/behaviour.hpp>
#include <physics/collider.hpp>
#include <gl/camera.hpp>
#include <gl/light.hpp>
#include <gl/renderable.hpp>

namespace {
    flecs::world* g_world = nullptr;
}

flecs::world* vpg::ecs::get_world() {
    return g_world;
}

void vpg::ecs::bind_world(flecs::world* world) {
    g_world = world;
}

vpg::ecs::Entity vpg::ecs::create_entity() {
    if (g_world == nullptr) {
        std::cerr << "vpg::ecs::create_entity() failed:\n"
                  << "Flecs world not bound\n";
        abort();
    }

    return g_world->entity().id();
}

void vpg::ecs::destroy_entity(Entity entity) {
    if (g_world == nullptr || entity == NullEntity || !g_world->is_alive(entity)) {
        return;
    }

    g_world->entity(entity).destruct();
}

bool vpg::ecs::add_component(Entity entity, vpg::memory::Stream& stream) {
    auto type = stream.read_string();

    if (type == Transform::TypeName) {
        Transform::Info info;
        if (!info.deserialize(stream) || stream.failed()) {
            return false;
        }
        if (g_world == nullptr || entity == NullEntity) {
            std::cerr << "vpg::ecs::add_component() failed:\n"
                      << "World not bound or entity is null\n";
            return false;
        }
        auto e = g_world->entity(entity);
        if (e.has<Transform>()) {
            e.remove<Transform>();
        }
        e.emplace<Transform>(entity, info);
        return true;
    }

    if (type == Behaviour::TypeName) {
        Behaviour::Info info;
        if (!info.deserialize(stream) || stream.failed()) {
            return false;
        }
        if (g_world == nullptr || entity == NullEntity) {
            std::cerr << "vpg::ecs::add_component() failed:\n"
                      << "World not bound or entity is null\n";
            return false;
        }
        auto e = g_world->entity(entity);
        if (e.has<Behaviour>()) {
            e.remove<Behaviour>();
        }
        e.emplace<Behaviour>(entity, info);
        return true;
    }

    if (type == vpg::physics::Collider::TypeName) {
        vpg::physics::Collider::Info info;
        if (!info.deserialize(stream) || stream.failed()) {
            return false;
        }
        if (g_world == nullptr || entity == NullEntity) {
            std::cerr << "vpg::ecs::add_component() failed:\n"
                      << "World not bound or entity is null\n";
            return false;
        }
        auto e = g_world->entity(entity);
        if (e.has<vpg::physics::Collider>()) {
            e.remove<vpg::physics::Collider>();
        }
        e.emplace<vpg::physics::Collider>(entity, info);
        return true;
    }

    if (type == vpg::gl::Camera::TypeName) {
        vpg::gl::Camera::Info info;
        if (!info.deserialize(stream) || stream.failed()) {
            return false;
        }
        if (g_world == nullptr || entity == NullEntity) {
            std::cerr << "vpg::ecs::add_component() failed:\n"
                      << "World not bound or entity is null\n";
            return false;
        }
        auto e = g_world->entity(entity);
        if (e.has<vpg::gl::Camera>()) {
            e.remove<vpg::gl::Camera>();
        }
        e.emplace<vpg::gl::Camera>(entity, info);
        return true;
    }

    if (type == vpg::gl::Light::TypeName) {
        vpg::gl::Light::Info info;
        if (!info.deserialize(stream) || stream.failed()) {
            return false;
        }
        if (g_world == nullptr || entity == NullEntity) {
            std::cerr << "vpg::ecs::add_component() failed:\n"
                      << "World not bound or entity is null\n";
            return false;
        }
        auto e = g_world->entity(entity);
        if (e.has<vpg::gl::Light>()) {
            e.remove<vpg::gl::Light>();
        }
        e.emplace<vpg::gl::Light>(entity, info);
        return true;
    }

    if (type == vpg::gl::Renderable::TypeName) {
        vpg::gl::Renderable::Info info;
        if (!info.deserialize(stream) || stream.failed()) {
            return false;
        }
        if (g_world == nullptr || entity == NullEntity) {
            std::cerr << "vpg::ecs::add_component() failed:\n"
                      << "World not bound or entity is null\n";
            return false;
        }
        auto e = g_world->entity(entity);
        if (e.has<vpg::gl::Renderable>()) {
            e.remove<vpg::gl::Renderable>();
        }
        e.emplace<vpg::gl::Renderable>(entity, info);
        return true;
    }

    std::cerr << "vpg::ecs::add_component() failed:\n"
              << "Component type \"" << type << "\" isn't supported\n";
    return false;
}


