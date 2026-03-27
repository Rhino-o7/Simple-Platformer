#pragma once

#include <string>
#include <unordered_map>

namespace game {
    struct SceneManifest {
        std::string entry_scene;
        std::unordered_map<std::string, std::string> scenes;

        bool try_get_asset(const std::string& scene_name, std::string& out_asset_id) const;
    };

    class SceneManifestLoader {
    public:
        static bool parse(const std::string& json, SceneManifest& out_manifest);
    };
}
