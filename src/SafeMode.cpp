#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include "State.hpp"

using namespace geode::prelude;

// Safe mode: nothing from the attempt gets saved, no percentage, new best
// or completion. GD already skips saving for start position runs
// (m_isTestMode), so we pretend to be one while it decides what to save.
//
// It's forced on while any hack GD can't detect is active (click buffer,
// frame advance, speedhack), so those can't produce legit-looking progress.
class $modify(SafeModePlayLayer, PlayLayer) {
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (!g_hacks.safeModeActive()) return PlayLayer::destroyPlayer(player, object);

        bool wasTestMode = m_isTestMode;
        m_isTestMode = true;
        PlayLayer::destroyPlayer(player, object);
        m_isTestMode = wasTestMode;
    }

    void levelComplete() {
        if (!g_hacks.safeModeActive()) return PlayLayer::levelComplete();

        bool wasTestMode = m_isTestMode;
        m_isTestMode = true;
        PlayLayer::levelComplete();
        m_isTestMode = wasTestMode;
    }
};
