#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include "Simulation.hpp"
#include "State.hpp"

using namespace geode::prelude;

// Click buffer (the "dynamic" cheat). A jump click that would do nothing
// right now, because the player is in the air on a ground-based gamemode
// with no orb to hit, isn't thrown away. It's held for up to
// `buffer-steps` physics steps and fired the first step it would do
// something: when the player lands or touches an orb. If that never happens
// in time, it fires anyway, so you never lose a click.
//
// Physics runs at 240 steps per second, so 15 steps is about 62ms, or about
// 4 frames at 60 FPS.
namespace {
    struct HeldClick {
        bool active = false;
        // The button went up while we were holding the push. Fire the push,
        // then send the release one step later.
        bool released = false;
        bool isPlayer2 = false;
        int age = 0;
    };

    bool clickWouldDoSomething(PlayerObject* player) {
        if (!player || player->m_isDead) return true;
        // Flying gamemodes react to every click.
        if (player->m_isShip || player->m_isBird || player->m_isDart || player->m_isSwing) return true;
        if (player->m_isOnGround) return true;
        return player->m_touchingRings && player->m_touchingRings->count() > 0;
    }
}

class $modify(ClickBufferLayer, GJBaseGameLayer) {
    struct Fields {
        HeldClick m_held[2];
        bool m_releaseNext[2] = { false, false };
        bool m_releaseNextIsPlayer2[2] = { false, false };
        int m_lastStep = 0;
    };

    int playerIndex(bool isPlayer2) {
        return isPlayer2 && m_gameState.m_isDualMode ? 1 : 0;
    }

    PlayerObject* playerAt(int index) {
        return index == 1 ? m_player2 : m_player1;
    }

    void sendJump(bool down, bool isPlayer2) {
        this->handleButton(down, static_cast<int>(PlayerButton::Jump), !isPlayer2);
    }

    // Drops held pushes and sends any release still owed, so GD never
    // thinks the button is stuck down.
    void clearHeld() {
        auto fields = m_fields.operator->();
        for (int i = 0; i < 2; i++) {
            fields->m_held[i] = {};
            if (fields->m_releaseNext[i]) {
                fields->m_releaseNext[i] = false;
                this->sendJump(false, fields->m_releaseNextIsPlayer2[i]);
            }
        }
    }

    void processQueuedButtons(float dt, bool clearInputQueue) {
        auto fields = m_fields.operator->();

        // Off, in the editor, or survival delay is handling inputs: behave
        // like vanilla. A step counter that went backwards means the level
        // restarted, so drop old clicks too.
        if (g_sim.active) return GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);
        bool active = g_hacks.clickBuffer && !g_hacks.survivalDelay && typeinfo_cast<PlayLayer*>(this);
        if (!active || m_currentStep < fields->m_lastStep) {
            this->clearHeld();
        }
        fields->m_lastStep = m_currentStep;
        if (!active) {
            return GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);
        }

        // 1. Pull out jump pushes that would be wasted right now, plus the
        //    releases that belong to them.
        for (auto it = m_queuedButtons.begin(); it != m_queuedButtons.end();) {
            if (it->m_button != PlayerButton::Jump) {
                ++it;
                continue;
            }
            int index = this->playerIndex(it->m_isPlayer2);
            auto& held = fields->m_held[index];

            if (held.active) {
                // Already holding a click for this player: swallow the
                // release (and any extra taps) until it fires.
                if (!it->m_isPush) held.released = true;
                it = m_queuedButtons.erase(it);
                continue;
            }
            if (it->m_isPush && !clickWouldDoSomething(this->playerAt(index))) {
                held = { true, false, it->m_isPlayer2, 0 };
                it = m_queuedButtons.erase(it);
                continue;
            }
            ++it;
        }

        GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);

        for (int i = 0; i < 2; i++) {
            // 2. Release a click we fired last step.
            if (fields->m_releaseNext[i]) {
                fields->m_releaseNext[i] = false;
                this->sendJump(false, fields->m_releaseNextIsPlayer2[i]);
            }

            // 3. Fire held clicks that can do something now, or ran out of time.
            auto& held = fields->m_held[i];
            if (!held.active) continue;
            held.age++;
            if (clickWouldDoSomething(this->playerAt(i)) || held.age > g_hacks.bufferSteps) {
                this->sendJump(true, held.isPlayer2);
                if (held.released) {
                    fields->m_releaseNext[i] = true;
                    fields->m_releaseNextIsPlayer2[i] = held.isPlayer2;
                }
                held = {};
            }
        }
    }
};
