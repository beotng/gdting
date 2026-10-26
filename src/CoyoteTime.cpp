#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include "Assist.hpp"
#include "Simulation.hpp"
#include "State.hpp"

using namespace geode::prelude;

// Coyote time: a jump pressed up to `coyote-steps` physics steps after
// walking off an edge still jumps, as if you'd pressed it on the edge. The
// late side of the input leeway; click buffer and survival delay handle
// early inputs.
//
// GD decides whether to jump inside PlayerObject::update (updateJump), from
// the ground flag, so we set that flag for the one step after the press.
// Only counts after walking off: leaving the ground by jumping, or by an
// orb or pad, doesn't open a coyote window.
class $modify(CoyotePlayer, PlayerObject) {
    struct Fields {
        // Steps since walking off the ground, or -1 if the player is on the
        // ground or left it some other way.
        int m_stepsSinceWalkOff = -1;
        bool m_forceGrounded = false;
    };

    bool isRealPlayer() {
        if (g_sim.active) return false;
        auto playLayer = PlayLayer::get();
        return playLayer && (this == playLayer->m_player1 || this == playLayer->m_player2);
    }

    void update(float dt) {
        if (!this->isRealPlayer()) return PlayerObject::update(dt);

        auto fields = m_fields.operator->();
        bool wasOnGround = m_isOnGround;
        if (std::exchange(fields->m_forceGrounded, false)) m_isOnGround = true;

        PlayerObject::update(dt);

        if (m_isOnGround) {
            fields->m_stepsSinceWalkOff = -1;
        }
        else if (wasOnGround) {
            // Moving away from the ground (up, or down when upside down)
            // means a jump or a launch, not walking off.
            double away = m_isUpsideDown ? -m_yVelocity : m_yVelocity;
            fields->m_stepsSinceWalkOff = away <= 0.0 ? 1 : -1;
        }
        else if (fields->m_stepsSinceWalkOff >= 0) {
            fields->m_stepsSinceWalkOff++;
        }
    }

    // Called on a jump press. Returns the coyote steps used, or -1.
    int tryCoyoteJump() {
        auto fields = m_fields.operator->();
        int steps = fields->m_stepsSinceWalkOff;
        if (steps < 0 || steps > g_hacks.coyoteSteps) return -1;
        if (m_isDead || m_isOnGround) return -1;
        if (m_isShip || m_isBird || m_isDart || m_isSwing) return -1;
        // Touching an orb: the press is for the orb.
        if (m_touchingRings && m_touchingRings->count() > 0) return -1;

        fields->m_forceGrounded = true;
        fields->m_stepsSinceWalkOff = -1;
        return steps;
    }
};

class $modify(CoyoteLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
        if (!down || button != static_cast<int>(PlayerButton::Jump)) return;
        if (!g_hacks.coyoteTime || g_sim.active || !typeinfo_cast<PlayLayer*>(this)) return;

        // Same rule as GD: in a two-player level each side moves its own
        // player, otherwise one press moves both in dual mode.
        bool dual = m_gameState.m_isDualMode && m_player2;
        bool twoPlayer = m_levelSettings && m_levelSettings->m_twoPlayerMode;
        bool moves1 = !twoPlayer || isPlayer1;
        bool moves2 = dual && (!twoPlayer || !isPlayer1);

        for (auto [moves, player] : { std::pair{ moves1, m_player1 }, std::pair{ moves2, m_player2 } }) {
            if (!moves || !player) continue;
            int steps = static_cast<CoyotePlayer*>(player)->tryCoyoteJump();
            if (steps >= 0) {
                reportAssist("Coyote", steps);
                log::info("Coyote time: jumped {} steps after walking off", steps);
            }
        }
    }
};
