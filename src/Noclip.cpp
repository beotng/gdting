#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include "Simulation.hpp"
#include "State.hpp"

using namespace geode::prelude;

// GD kills the player by calling destroyPlayer when they touch a hazard.
// Skipping it is all noclip needs. The one exception is the invisible
// anticheat spike GD hits at level start: let that one through so the game
// still flags the attempt as cheated instead of breaking.
//
// While SurvivalDelay is simulating ahead, a death is only recorded, never
// applied, whether noclip is on or not.
class $modify(NoclipPlayLayer, PlayLayer) {
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (g_sim.active) {
            if (object != m_anticheatSpike) g_sim.died = true;
            return;
        }
        if (g_hacks.noclip && object != m_anticheatSpike) {
            return;
        }
        PlayLayer::destroyPlayer(player, object);
    }
};
