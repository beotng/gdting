#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include "Simulation.hpp"
#include "State.hpp"

#include <deque>

using namespace geode::prelude;

// Survival delay. Every jump press or release is taken out of GD's input
// queue. Before it's applied, we save a checkpoint, apply it, simulate
// `delay-lookahead` physics steps ahead, and roll back. If the player lives,
// the input goes through right away. If they'd die, it waits a frame and
// tries again, up to `delay-frames` frames, then goes through anyway.
//
// Safe inputs are never delayed. Unsafe ones land on the first frame where
// they don't kill you, e.g. a wave click in a tight spam section.
namespace {
    constexpr float STEP = 1.f / 240.f;

    struct PendingInput {
        bool push;
        bool isPlayer2;
        int age = 0;
    };

    // Checkpoints don't store which buttons are held, so we save that ourselves.
    struct HeldState {
        gd::map<int, bool> buttons;
        bool jumpBuffered = false;
        bool wasJumpBuffered = false;
        unsigned char stateJumpBuffered = 0;
        bool holdingLeft = false;
        bool holdingRight = false;
    };

    HeldState saveHeld(PlayerObject* player) {
        if (!player) return {};
        return {
            player->m_holdingButtons,
            player->m_jumpBuffered,
            player->m_wasJumpBuffered,
            player->m_stateJumpBuffered,
            player->m_holdingLeft,
            player->m_holdingRight,
        };
    }

    void restoreHeld(PlayerObject* player, HeldState const& state) {
        if (!player) return;
        player->m_holdingButtons = state.buttons;
        player->m_jumpBuffered = state.jumpBuffered;
        player->m_wasJumpBuffered = state.wasJumpBuffered;
        player->m_stateJumpBuffered = state.stateJumpBuffered;
        player->m_holdingLeft = state.holdingLeft;
        player->m_holdingRight = state.holdingRight;
    }
}

class $modify(SurvivalDelayLayer, GJBaseGameLayer) {
    struct Fields {
        std::deque<PendingInput> m_pending;
    };

    void applyInput(PendingInput const& input) {
        this->handleButton(input.push, static_cast<int>(PlayerButton::Jump), !input.isPlayer2);
    }

    // Applies `input`, plays `delay-lookahead` steps ahead and rolls back.
    bool survivesIfApplied(PlayLayer* playLayer, PendingInput const& input) {
        Ref<CheckpointObject> checkpoint = playLayer->createCheckpoint();
        if (!checkpoint) return true;

        auto held1 = saveHeld(m_player1);
        auto held2 = saveHeld(m_player2);
        double extraDelta = m_extraDelta;
        int currentStep = m_currentStep;

        g_sim = { true, false };
        this->applyInput(input);
        for (int i = 0; i < g_hacks.delayLookahead && !g_sim.died; i++) {
            m_extraDelta = 0.0;
            GJBaseGameLayer::update(STEP * m_gameState.m_timeWarp);
        }
        bool died = g_sim.died;

        playLayer->loadFromCheckpoint(checkpoint);
        restoreHeld(m_player1, held1);
        restoreHeld(m_player2, held2);
        m_extraDelta = extraDelta;
        m_currentStep = currentStep;
        g_sim = {};

        return !died;
    }

    void update(float dt) {
        if (g_sim.active) return GJBaseGameLayer::update(dt);

        auto playLayer = typeinfo_cast<PlayLayer*>(this);
        auto& pending = m_fields->m_pending;
        bool active = g_hacks.survivalDelay && !g_hacks.frameAdvance && playLayer
            && !m_player1->m_isDead && !m_levelEndAnimationStarted;

        if (!active) {
            // Turned off mid-hold: don't lose inputs. Dead: they're stale.
            if (playLayer && !m_player1->m_isDead) {
                for (auto const& input : pending) this->applyInput(input);
            }
            pending.clear();
            return GJBaseGameLayer::update(dt);
        }

        // Take this frame's jump inputs out of GD's queue.
        for (auto it = m_queuedButtons.begin(); it != m_queuedButtons.end();) {
            if (it->m_button == PlayerButton::Jump) {
                pending.push_back({ it->m_isPush, it->m_isPlayer2 });
                it = m_queuedButtons.erase(it);
            }
            else {
                ++it;
            }
        }

        // Apply them in order, each one as soon as it doesn't kill you.
        while (!pending.empty()) {
            auto const& next = pending.front();
            if (next.age < g_hacks.delayFrames && !this->survivesIfApplied(playLayer, next)) break;
            this->applyInput(next);
            pending.pop_front();
        }
        for (auto& input : pending) input.age++;

        GJBaseGameLayer::update(dt);
    }
};

class $modify(SurvivalDelayPlayLayer, PlayLayer) {
    // The simulation reaching the end of the level counts as surviving, and
    // must not actually complete it.
    void levelComplete() {
        if (g_sim.active) return;
        PlayLayer::levelComplete();
    }
};
