#include <data/text.hpp>

#include "game_manager.hpp"
#include "behaviour_registry.hpp"

using namespace vpg;
bool load_game(game::runtime::SceneState* scene) {
    game::register_behaviours();

    Manager::scene = scene;
    return Manager::load();
}





