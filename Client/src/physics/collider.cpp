#include <physics/collider.hpp>

using namespace vpg;
using namespace vpg::physics;

bool Collider::Info::serialize(memory::Stream& stream) const {
    stream.write_comment("Collider", 0);
    stream.write_string(this->is_static ? "Static" : "Dynamic");
    if (this->type == Type::Sphere) {
        stream.write_string("Sphere");
        stream.write_comment("Radius", 1);
        stream.write_f32(this->sphere.radius);
    }
    else if (this->type == Type::AABB) {
        stream.write_string("AABB");
        stream.write_comment("Min", 1);
        stream.write_f32(this->aabb.min.x);
        stream.write_f32(this->aabb.min.y);
        stream.write_f32(this->aabb.min.z);
        stream.write_comment("Max", 1);
        stream.write_f32(this->aabb.max.x);
        stream.write_f32(this->aabb.max.y);
        stream.write_f32(this->aabb.max.z);
    }
    else {
        std::cerr << "vpg::physics::Collider::Info::serialize() failed:\n"
                  << "Invalid collider type\n";
        return false;
    }

    return !stream.failed();
}

bool Collider::Info::deserialize(memory::Stream& stream) {
    std::string is_static = stream.read_string();
    std::string str_type = stream.read_string();
    if (is_static == "Static") {
        this->is_static = true;
    }
    else if (is_static == "Dynamic") {
        this->is_static = false;
    }
    else {
        std::cerr << "vpg::physics::Collider::Info::deserialize() failed:\n"
                  << "Expected 'Static' or 'Dynamic', found '" << is_static << "'\n";
        return false;
    }

    if (str_type == "Sphere") {
        this->type = Type::Sphere;
        this->sphere.radius = stream.read_f32();
    }
    else if (str_type == "AABB") {
        this->type = Type::AABB;
        this->aabb.min.x = stream.read_f32();
        this->aabb.min.y = stream.read_f32();
        this->aabb.min.z = stream.read_f32();
        this->aabb.max.x = stream.read_f32();
        this->aabb.max.y = stream.read_f32();
        this->aabb.max.z = stream.read_f32();
    }
    else {
        std::cerr << "vpg::physics::Collider::Info::deserialize() failed:\n"
                  << "Unsupported collider type '" << str_type << "'\n";
        return false;
    }

    return !stream.failed();
}

Collider::Collider(ecs::Entity entity, const Info& create_info) {
    this->type = create_info.type;
    this->is_static = create_info.is_static;
    this->sphere = create_info.sphere;
    this->aabb = create_info.aabb;
}

