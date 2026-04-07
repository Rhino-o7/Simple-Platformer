#pragma once

#include <cstdint>
#include <memory/stream.hpp>

#ifndef flecs_STATIC
#define flecs_STATIC
#endif

#include <flecs.h>

#include <iostream>

namespace vpg::ecs {
    using Entity = flecs::entity_t;
    constexpr Entity NullEntity = 0;

    flecs::world* get_world();
    void bind_world(flecs::world* world);

    bool add_component(Entity entity, memory::Stream& stream);

    Entity create_entity();
    void destroy_entity(Entity entity);

    // Direct FLECS access helpers
    template<typename T>
    inline T* get_component(Entity entity) {
        auto world = get_world();
        if (world == nullptr || entity == NullEntity || !world->is_alive(entity)) {
            return nullptr;
        }

        auto e = world->entity(entity);
        if (!e.has<T>()) {
            return nullptr;
        }

        return e.try_get_mut<T>();
    }

    template<typename T>
    inline void remove_component(Entity entity) {
        auto world = get_world();
        if (world == nullptr || entity == NullEntity) {
            return;
        }

        auto e = world->entity(entity);
        if (e.has<T>()) {
            e.remove<T>();
        }
    }

    template<typename T>
    inline T& add_component(Entity entity, const typename T::Info& create_info) {
        auto world = get_world();
        if (world == nullptr || entity == NullEntity) {
            std::cerr << "vpg::ecs::add_component() failed:\n"
                      << "World not bound or entity is null\n";
            abort();
        }

        auto e = world->entity(entity);
        if (e.has<T>()) {
            e.remove<T>();
        }

        e.emplace<T>(entity, create_info);
        return *e.try_get_mut<T>();
    }
}


