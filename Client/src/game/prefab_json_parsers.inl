    using EntityLookup = std::unordered_map<std::string, vpg::ecs::Entity>;
    using BehaviourParser = bool (*)(vpg::ecs::Entity, const JsonValue&, const EntityLookup&, std::string&);

    bool resolve_entity_ref(const EntityLookup& entities, const std::string& name, vpg::ecs::Entity& out_ref, std::string& error) {
        auto it = entities.find(name);
        if (it == entities.end()) {
            error = "Unknown entity reference '" + name + "'";
            return false;
        }
        out_ref = it->second;
        return true;
    }

    bool parse_platform_behaviour(vpg::ecs::Entity entity, const JsonValue& behaviour, const EntityLookup&, std::string& error) {
        Platform::Info info = {};
        if (!as_vec3(get_field(behaviour, "from"), info.from)
            || !as_vec3(get_field(behaviour, "to"), info.to)
            || !as_float(get_field(behaviour, "speed"), info.speed)) {
            error = "Platform behaviour requires from/to/speed";
            return false;
        }
        vpg::ecs::add_component<vpg::ecs::Behaviour>(entity, vpg::ecs::Behaviour::Info::create<Platform>(std::move(info)));
        return true;
    }

    bool parse_turret_behaviour(vpg::ecs::Entity entity, const JsonValue& behaviour, const EntityLookup&, std::string& error) {
        Turret::Info info = {};
        std::string bullet_asset;
        if (!as_string(get_field(behaviour, "bulletAsset"), bullet_asset)
            || !as_float(get_field(behaviour, "delay"), info.delay)
            || !as_float(get_field(behaviour, "speed"), info.speed)) {
            error = "Turret behaviour requires bulletAsset/delay/speed";
            return false;
        }
        info.bullet = vpg::data::Manager::load<vpg::data::Text>(bullet_asset);
        if (info.bullet.get_asset() == nullptr) {
            error = "Invalid turret bullet asset '" + bullet_asset + "'";
            return false;
        }
        vpg::ecs::add_component<vpg::ecs::Behaviour>(entity, vpg::ecs::Behaviour::Info::create<Turret>(std::move(info)));
        return true;
    }

    bool parse_firetrap_behaviour(vpg::ecs::Entity entity, const JsonValue& behaviour, const EntityLookup&, std::string& error) {
        Firetrap::Info info = {};
        std::string smoke_asset, spread_asset;
        if (!as_string(get_field(behaviour, "smokeAsset"), smoke_asset)
            || !as_string(get_field(behaviour, "firespreadAsset"), spread_asset)
            || !as_float(get_field(behaviour, "recoil"), info.recoil)
            || !as_float(get_field(behaviour, "delay"), info.delay)
            || !as_float(get_field(behaviour, "time"), info.time)
            || !as_vec3(get_field(behaviour, "center"), info.center)) {
            error = "Firetrap behaviour missing required fields";
            return false;
        }
        as_float(get_field(behaviour, "seed"), info.seed);
        info.smoke = vpg::data::Manager::load<vpg::data::Text>(smoke_asset);
        info.firespread = vpg::data::Manager::load<vpg::data::Text>(spread_asset);
        if (info.smoke.get_asset() == nullptr || info.firespread.get_asset() == nullptr) {
            error = "Invalid firetrap asset reference";
            return false;
        }
        vpg::ecs::add_component<vpg::ecs::Behaviour>(entity, vpg::ecs::Behaviour::Info::create<Firetrap>(std::move(info)));
        return true;
    }

    bool parse_firespread_behaviour(vpg::ecs::Entity entity, const JsonValue& behaviour, const EntityLookup&, std::string& error) {
        Firespread::Info info = {};
        std::string smoke_asset;
        if (!as_string(get_field(behaviour, "smokeAsset"), smoke_asset)
            || !as_float(get_field(behaviour, "recoil"), info.recoil)
            || !as_float(get_field(behaviour, "delay"), info.delay)
            || !as_float(get_field(behaviour, "time"), info.time)
            || !as_vec3(get_field(behaviour, "center"), info.center)) {
            error = "Firespread behaviour missing required fields";
            return false;
        }
        as_float(get_field(behaviour, "seed"), info.seed);
        info.smoke = vpg::data::Manager::load<vpg::data::Text>(smoke_asset);
        if (info.smoke.get_asset() == nullptr) {
            error = "Invalid firespread smoke asset";
            return false;
        }
        vpg::ecs::add_component<vpg::ecs::Behaviour>(entity, vpg::ecs::Behaviour::Info::create<Firespread>(std::move(info)));
        return true;
    }

    bool parse_smoke_behaviour(vpg::ecs::Entity entity, const JsonValue& behaviour, const EntityLookup&, std::string& error) {
        Smoke::Info info = {};
        if (!as_vec3(get_field(behaviour, "center"), info.center)
            || !as_float(get_field(behaviour, "speed"), info.speed)) {
            error = "Smoke behaviour requires center/speed";
            return false;
        }
        as_float(get_field(behaviour, "time"), info.time);
        vpg::ecs::add_component<vpg::ecs::Behaviour>(entity, vpg::ecs::Behaviour::Info::create<Smoke>(std::move(info)));
        return true;
    }

    bool parse_bullet_behaviour(vpg::ecs::Entity entity, const JsonValue&, const EntityLookup&, std::string&) {
        Bullet::Info info = {};
        vpg::ecs::add_component<vpg::ecs::Behaviour>(entity, vpg::ecs::Behaviour::Info::create<Bullet>(std::move(info)));
        return true;
    }

    bool parse_player_controller_behaviour(vpg::ecs::Entity entity, const JsonValue& behaviour, const EntityLookup& entities, std::string& error) {
        PlayerController::Info info = {};
        std::string torso, lfoot, rfoot, lhand, rhand, feet;
        if (!as_string(get_field(behaviour, "torso"), torso)
            || !as_string(get_field(behaviour, "lfoot"), lfoot)
            || !as_string(get_field(behaviour, "rfoot"), rfoot)
            || !as_string(get_field(behaviour, "lhand"), lhand)
            || !as_string(get_field(behaviour, "rhand"), rhand)
            || !as_string(get_field(behaviour, "feetCollider"), feet)) {
            error = "PlayerController behaviour missing entity references";
            return false;
        }

        if (!resolve_entity_ref(entities, torso, info.torso, error)
            || !resolve_entity_ref(entities, lfoot, info.lfoot, error)
            || !resolve_entity_ref(entities, rfoot, info.rfoot, error)
            || !resolve_entity_ref(entities, lhand, info.lhand, error)
            || !resolve_entity_ref(entities, rhand, info.rhand, error)
            || !resolve_entity_ref(entities, feet, info.feet_collider, error)) {
            return false;
        }

        vpg::ecs::add_component<vpg::ecs::Behaviour>(entity, vpg::ecs::Behaviour::Info::create<PlayerController>(std::move(info)));
        return true;
    }

    bool parse_player_instance_behaviour(vpg::ecs::Entity entity, const JsonValue& behaviour, const EntityLookup& entities, std::string& error) {
        PlayerInstance::Info info = {};
        std::string prefab_asset, camera_ref;
        const JsonValue* prefab_asset_value = get_field(behaviour, "prefabAsset");
        if (prefab_asset_value == nullptr) {
            prefab_asset_value = get_field(behaviour, "sceneAsset");
        }

        if (!as_string(prefab_asset_value, prefab_asset)
            || !as_vec3(get_field(behaviour, "position"), info.position)
            || !as_string(get_field(behaviour, "camera"), camera_ref)) {
            error = "PlayerInstance behaviour missing required fields";
            return false;
        }
        info.scene = vpg::data::Manager::load<vpg::data::Text>(prefab_asset);
        if (info.scene.get_asset() == nullptr) {
            error = "Invalid PlayerInstance prefab asset";
            return false;
        }

        if (!resolve_entity_ref(entities, camera_ref, info.camera, error)) {
            error = "Unknown camera ref '" + camera_ref + "'";
            return false;
        }

        vpg::ecs::add_component<vpg::ecs::Behaviour>(entity, vpg::ecs::Behaviour::Info::create<PlayerInstance>(std::move(info)));
        return true;
    }

    bool parse_map_controller_behaviour(vpg::ecs::Entity entity, const JsonValue& behaviour, const EntityLookup& entities, std::string& error) {
        MapController::Info info = {};
        std::string player_ref, kill_ref;
        auto prefabs_field = get_field(behaviour, "prefabs");
        if (!as_string(get_field(behaviour, "player"), player_ref)
            || !as_string(get_field(behaviour, "killArea"), kill_ref)
            || prefabs_field == nullptr
            || !prefabs_field->IsObject()) {
            error = "MapController behaviour missing required fields";
            return false;
        }

        auto p_it = entities.find(player_ref);
        auto k_it = entities.find(kill_ref);
        if (p_it == entities.end() || k_it == entities.end()) {
            error = "MapController has invalid entity reference";
            return false;
        }

        info.player = p_it->second;
        info.kill_area = k_it->second;
        for (auto it = prefabs_field->MemberBegin(); it != prefabs_field->MemberEnd(); ++it) {
            if (!it->value.IsString()) {
                error = "MapController prefab mapping values must be strings";
                return false;
            }
            auto asset = vpg::data::Manager::load<vpg::data::Text>(it->value.GetString());
            if (asset.get_asset() == nullptr) {
                error = "Invalid MapController prefab asset '" + std::string(it->value.GetString()) + "'";
                return false;
            }
            info.prefabs[it->name.GetString()] = asset;
        }

        vpg::ecs::add_component<vpg::ecs::Behaviour>(entity, vpg::ecs::Behaviour::Info::create<MapController>(std::move(info)));
        return true;
    }

    bool parse_behaviour(
        vpg::ecs::Entity entity,
        const JsonValue& behaviour,
        const EntityLookup& entities,
        std::string& error) {
        std::string type;
        if (!as_string(get_field(behaviour, "type"), type)) {
            error = "Behaviour component requires string field 'type'";
            return false;
        }

        static const std::unordered_map<std::string, BehaviourParser> behaviour_parsers = {
            { Platform::TypeName, parse_platform_behaviour },
            { Turret::TypeName, parse_turret_behaviour },
            { Firetrap::TypeName, parse_firetrap_behaviour },
            { Firespread::TypeName, parse_firespread_behaviour },
            { Smoke::TypeName, parse_smoke_behaviour },
            { Bullet::TypeName, parse_bullet_behaviour },
            { PlayerController::TypeName, parse_player_controller_behaviour },
            { PlayerInstance::TypeName, parse_player_instance_behaviour },
            { MapController::TypeName, parse_map_controller_behaviour },
        };

        auto parser = behaviour_parsers.find(type);
        if (parser == behaviour_parsers.end()) {
            error = "Unsupported behaviour type '" + type + "'";
            return false;
        }

        return parser->second(entity, behaviour, entities, error);
    }

    using PendingParents = std::vector<std::pair<vpg::ecs::Entity, std::string>>;
    using ComponentParser = bool (*)(vpg::ecs::Entity, const JsonValue&, const EntityLookup&, PendingParents&, std::string&);

    bool parse_transform_component(vpg::ecs::Entity entity, const JsonValue& transform, const EntityLookup&, PendingParents& pending_parents, std::string& error) {
        if (!transform.IsObject()) {
            error = "transform must be object";
            return false;
        }

        vpg::ecs::Transform::Info t_info = {};
        t_info.parent = vpg::ecs::NullEntity;
        t_info.scale = { 1.0f, 1.0f, 1.0f };
        t_info.rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
        std::string parent_name;

        as_vec3(get_field(transform, "position"), t_info.position);
        as_vec3(get_field(transform, "scale"), t_info.scale);
        as_quat(get_field(transform, "rotation"), t_info.rotation);

        auto parent_field = get_field(transform, "parent");
        if (parent_field != nullptr && parent_field->IsString()) {
            parent_name = parent_field->GetString();
        }

        vpg::ecs::add_component<vpg::ecs::Transform>(entity, t_info);
        pending_parents.emplace_back(entity, parent_name);
        return true;
    }

    bool parse_collider_component(vpg::ecs::Entity entity, const JsonValue& collider, const EntityLookup&, PendingParents&, std::string& error) {
        if (!collider.IsObject()) {
            error = "collider must be object";
            return false;
        }

        vpg::physics::Collider::Info c_info = {};
        std::string body, shape;
        if (!as_string(get_field(collider, "body"), body) || !as_string(get_field(collider, "shape"), shape)) {
            error = "Collider requires body and shape";
            return false;
        }

        c_info.is_static = (body == "Static");
        if (shape == "AABB") {
            c_info.type = vpg::physics::Collider::Type::AABB;
            if (!as_vec3(get_field(collider, "min"), c_info.aabb.min) || !as_vec3(get_field(collider, "max"), c_info.aabb.max)) {
                error = "AABB collider requires min/max";
                return false;
            }
        }
        else if (shape == "Sphere") {
            c_info.type = vpg::physics::Collider::Type::Sphere;
            if (!as_float(get_field(collider, "radius"), c_info.sphere.radius)) {
                error = "Sphere collider requires radius";
                return false;
            }
        }
        else {
            error = "Unsupported collider shape '" + shape + "'";
            return false;
        }

        vpg::ecs::add_component<vpg::physics::Collider>(entity, c_info);
        return true;
    }

    bool parse_renderable_component(vpg::ecs::Entity entity, const JsonValue& renderable, const EntityLookup&, PendingParents&, std::string& error) {
        if (!renderable.IsObject()) {
            error = "renderable must be object";
            return false;
        }

        std::string model_id;
        if (!as_string(get_field(renderable, "model"), model_id)) {
            error = "Renderable requires model string";
            return false;
        }

        vpg::gl::Renderable::Info r_info = {};
        r_info.type = vpg::gl::Renderable::Type::Model;
        r_info.model = vpg::data::Manager::load<vpg::data::Model>(model_id);
        if (r_info.model.get_asset() == nullptr) {
            error = "Renderable model asset not found: '" + model_id + "'";
            return false;
        }

        vpg::ecs::add_component<vpg::gl::Renderable>(entity, r_info);
        return true;
    }

    bool parse_light_component(vpg::ecs::Entity entity, const JsonValue& light, const EntityLookup&, PendingParents&, std::string& error) {
        if (!light.IsObject()) {
            error = "light must be object";
            return false;
        }

        vpg::gl::Light::Info l_info = {};
        std::string type;
        if (!as_string(get_field(light, "type"), type)) {
            error = "Light requires type";
            return false;
        }

        if (type == "Point") {
            l_info.type = vpg::gl::Light::Type::Point;
        }
        else if (type == "Directional") {
            l_info.type = vpg::gl::Light::Type::Directional;
        }
        else {
            error = "Unsupported light type '" + type + "'";
            return false;
        }

        as_vec3(get_field(light, "ambient"), l_info.ambient);
        as_vec3(get_field(light, "diffuse"), l_info.diffuse);
        as_float(get_field(light, "constant"), l_info.constant);
        as_float(get_field(light, "linear"), l_info.linear);
        as_float(get_field(light, "quadratic"), l_info.quadratic);
        vpg::ecs::add_component<vpg::gl::Light>(entity, l_info);
        return true;
    }

    bool parse_camera_component(vpg::ecs::Entity entity, const JsonValue& camera, const EntityLookup&, PendingParents&, std::string& error) {
        if (!camera.IsObject()) {
            error = "camera must be object";
            return false;
        }

        vpg::gl::Camera::Info c_info = {};
        as_float(get_field(camera, "fov"), c_info.fov);
        as_float(get_field(camera, "zNear"), c_info.z_near);
        as_float(get_field(camera, "zFar"), c_info.z_far);
        vpg::ecs::add_component<vpg::gl::Camera>(entity, c_info);
        return true;
    }

    bool parse_behaviour_component(vpg::ecs::Entity entity, const JsonValue& behaviour, const EntityLookup& entities, PendingParents&, std::string& error) {
        if (!behaviour.IsObject()) {
            error = "behaviour must be object";
            return false;
        }

        return parse_behaviour(entity, behaviour, entities, error);
    }
