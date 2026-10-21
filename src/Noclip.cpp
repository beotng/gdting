#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include "State.hpp"

using namespace geode::prelude;

// GD kills the player by calling destroyPlayer when they touch a hazard.
// Skipping it is all noclip needs. The one exception is the invisible
// anticheat spike GD hits at level start: let that one through so the game
// still flags the attempt as cheated instead of breaking.
class $modify(NoclipPlayLayer, PlayLayer) {
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (g_hacks.noclip && object != m_anticheatSpike) {
            return;
        }
        PlayLayer::destroyPlayer(player, object);
    }
};
