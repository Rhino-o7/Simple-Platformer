#pragma once

#include <ecs/entity_manager.hpp>
#include <memory/stream.hpp>

#include <queue>
#include <unordered_map>
#include <vector>
#include <functional>
#include <tuple>
#include <iostream>

namespace vpg::ecs {
    class Coordinator {
    public:
        Coordinator() = delete;
        ~Coordinator() = delete;

        static void init();
        static void terminate();

        static Entity create_entity();
        static void destroy_entity(Entity entity);

        template<typename T>
        static void register_component();
        
        template<typename T>
        static T& add_component(Entity entity, const typename T::Info& create_info);
        static bool add_component(Entity entity, memory::Stream& stream);

        template<typename T>
        static void remove_component(Entity entity);

        template<typename T>
        static T* get_component(Entity entity);

    private:
        static bool initialized;
        static std::queue<Entity> available;
        static size_t count;
        static std::unordered_map<std::string, std::function<bool(Entity, memory::Stream&)>> stream_constructors;
        static std::vector<std::function<void(Entity)>> component_erasers;

        template<typename T>
        static std::unordered_map<Entity, T>& store();
    };

    template<typename T>
    inline std::unordered_map<Entity, T>& Coordinator::store() {
        static std::unordered_map<Entity, T> components;
        return components;
    }

    template<typename T>
    inline void Coordinator::register_component() {
        auto type_name = std::string(T::TypeName);
        if (Coordinator::stream_constructors.find(type_name) != Coordinator::stream_constructors.end()) {
            std::cerr << "vpg::ecs::Coordinator::register_component() failed:\n"
                      << "Component type already registered\n";
            abort();
        }

        Coordinator::stream_constructors.emplace(type_name, [](Entity entity, memory::Stream& stream) -> bool {
            typename T::Info create_info;
            if (!create_info.deserialize(stream) || stream.failed()) {
                return false;
            }

            Coordinator::add_component<T>(entity, create_info);
            return true;
        });

        Coordinator::component_erasers.emplace_back([](Entity entity) {
            Coordinator::store<T>().erase(entity);
        });
    }

    template<typename T>
    inline T& Coordinator::add_component(Entity entity, const typename T::Info& create_info) {
        auto& components = Coordinator::store<T>();
        auto existing = components.find(entity);
        if (existing != components.end()) {
            components.erase(existing);
        }

        auto [it, _] = components.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(entity),
            std::forward_as_tuple(entity, create_info)
        );
        return it->second;
    }

    template<typename T>
    inline void Coordinator::remove_component(Entity entity) {
        Coordinator::store<T>().erase(entity);
    }
    
    template<typename T>
    inline T* Coordinator::get_component(Entity entity) {
        auto& components = Coordinator::store<T>();
        auto it = components.find(entity);
        if (it == components.end()) {
            return nullptr;
        }

        return &it->second;
    }
    
}

