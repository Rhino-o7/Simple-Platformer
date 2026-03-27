#include <ecs/coordinator.hpp>

using namespace vpg::ecs;

bool Coordinator::initialized = false;
std::queue<Entity> Coordinator::available = {};
size_t Coordinator::count = 0;
std::unordered_map<std::string, std::function<bool(Entity, vpg::memory::Stream&)>> Coordinator::stream_constructors = {};
std::vector<std::function<void(Entity)>> Coordinator::component_erasers = {};

void Coordinator::init() {
    while (!Coordinator::available.empty()) {
        Coordinator::available.pop();
    }

    for (Entity i = 0; i < MaxEntities; ++i) {
        Coordinator::available.push(i);
    }

    Coordinator::count = 0;
    Coordinator::initialized = true;
}

void Coordinator::terminate() {
    Coordinator::initialized = false;

    while (!Coordinator::available.empty()) {
        Coordinator::available.pop();
    }

    Coordinator::count = 0;
    Coordinator::stream_constructors.clear();
    Coordinator::component_erasers.clear();
}

Entity Coordinator::create_entity() {
    if (!Coordinator::initialized || Coordinator::count >= MaxEntities || Coordinator::available.empty()) {
        std::cerr << "vpg::ecs::Coordinator::create_entity() failed:\n"
                  << "Entity pool exhausted or coordinator not initialized\n";
        abort();
    }

    auto entity = Coordinator::available.front();
    Coordinator::available.pop();
    Coordinator::count += 1;
    return entity;
}

void Coordinator::destroy_entity(Entity entity) {
    for (auto& erase_component : Coordinator::component_erasers) {
        erase_component(entity);
    }

    if (Coordinator::count > 0) {
        Coordinator::count -= 1;
    }

    Coordinator::available.push(entity);
}

bool Coordinator::add_component(Entity entity, vpg::memory::Stream& stream) {
    auto type = stream.read_string();
    auto it = Coordinator::stream_constructors.find(type);
    if (it == Coordinator::stream_constructors.end()) {
        std::cerr << "vpg::ecs::Coordinator::add_component() failed:\n"
                  << "Component type \"" << type << "\" isn't registered\n";
        return false;
    }

    if (!it->second(entity, stream)) {
        return false;
    }

    return true;
}

