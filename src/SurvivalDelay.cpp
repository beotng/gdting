#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include "Simulation.hpp"
#include "State.hpp"

#include <algorithm>
#include <deque>
#include <string>

using namespace geode::prelude;

// Survival delay. Every jump press or release is taken out of GD's input
// queue and checked before it's applied: an invisible copy of the player is
// stepped `delay-lookahead` physics steps ahead against the level's hitboxes,
// with the input applied now, a little later, or never.
//
//  - Safe now: applied right away.
//  - Deadly now, but safe if applied a few steps later (e.g. a jump pressed
//    slightly too early): held and re-checked every frame, for up to
//    `delay-steps` physics steps in total.
//  - Never safe within that window: a press is cancelled (with its release)
//    if not jumping survives; otherwise it goes through, since nothing helps.
//    Releases always go through eventually, so you never get stuck holding.
//
// The real game is never stepped or rolled back, so nothing in the level can
// get out of sync. The copy only collides with the level; anything it touches
// that would change the real game (triggers, coins, portals that move the
// camera, end of level) is ignored while simulating.
//
// Decisions and deaths are written to the Geode log ("Survival delay: ...")
// so its behaviour can be checked after playing.
namespace {
    // GD's real per-step delta, recorded from the real player each step.
    float g_stepDelta = 0.25f;

    struct PendingInput {
        bool push;
        bool isPlayer2;
        int queuedStep;
        bool held = false;
    };

    // The last decision, to explain the next death in the log.
    std::string g_lastDecision = "none";
    int g_lastDecisionStep = 0;

    bool isSim(PlayerObject* player) {
        return g_sim.active && player == g_sim.player;
    }
}

class $modify(SurvivalDelayLayer, GJBaseGameLayer) {
    struct Fields {
        std::deque<PendingInput> m_pending;
        Ref<CCLayer> m_simHolder;
        Ref<PlayerObject> m_simPlayer;
        // A cancelled press also cancels its release, per player.
        bool m_droppedPush[2] = { false, false };
    };

    PlayerObject* getSimPlayer() {
        auto fields = m_fields.operator->();
        if (!fields->m_simPlayer) {
            // Kept in a hidden holder so its particles never show up either.
            fields->m_simHolder = CCLayer::create();
            fields->m_simHolder->setVisible(false);
            m_objectLayer->addChild(fields->m_simHolder);

            fields->m_simPlayer = PlayerObject::create(1, 1, this, fields->m_simHolder, true);
            fields->m_simPlayer->setVisible(false);
            fields->m_simHolder->addChild(fields->m_simPlayer);
        }
        return fields->m_simPlayer;
    }

    // Steps a copy of `real` ahead, applying a jump input after `applyAt`
    // steps (never if negative), then `delay-lookahead` more steps.
    // Returns whether it's still alive at the end.
    bool simulate(PlayerObject* real, int applyAt, bool push) {
        if (!real || real->m_isDead) return true;

        auto sim = this->getSimPlayer();
        sim->copyAttributes(real);
        sim->setPosition(real->getPosition());
        sim->m_holdingButtons = real->m_holdingButtons;
        sim->m_jumpBuffered = real->m_jumpBuffered;
        sim->m_isDead = false;

        g_sim = { true, false, sim };
        int total = std::max(applyAt, 0) + g_hacks.delayLookahead;
        for (int i = 0; i < total && !g_sim.died; i++) {
            if (i == applyAt) {
                if (push) sim->pushButton(PlayerButton::Jump);
                else sim->releaseButton(PlayerButton::Jump);
            }
            sim->update(g_stepDelta);
            this->checkCollisions(sim, g_stepDelta, false);
            sim->updateRotation(g_stepDelta);
        }
        bool died = g_sim.died;
        g_sim = {};
        return !died;
    }

    // Which real players a jump input moves.
    std::pair<bool, bool> affectedPlayers(PendingInput const& input) {
        bool dual = m_gameState.m_isDualMode && m_player2;
        bool twoPlayer = m_levelSettings && m_levelSettings->m_twoPlayerMode;
        bool p1 = !twoPlayer || !input.isPlayer2;
        bool p2 = dual && (!twoPlayer || input.isPlayer2);
        return { p1, p2 };
    }

    bool survivesAt(PendingInput const& input, int applyAt) {
        auto [p1, p2] = this->affectedPlayers(input);
        if (p1 && !this->simulate(m_player1, applyAt, input.push)) return false;
        if (p2 && !this->simulate(m_player2, applyAt, input.push)) return false;
        return true;
    }

    // Whether holding the input for some of the remaining steps would make
    // it safe. Checks at most ~6 evenly spaced timings to stay cheap.
    bool waitingHelps(PendingInput const& input, int remaining) {
        if (remaining <= 0) return false;
        int stride = std::max(1, remaining / 6);
        for (int k = stride; k <= remaining; k += stride) {
            if (this->survivesAt(input, k)) return true;
        }
        return false;
    }

    void applyInput(PendingInput const& input) {
        this->handleButton(input.push, static_cast<int>(PlayerButton::Jump), !input.isPlayer2);
    }

    void record(PendingInput const& input, std::string const& what, bool print) {
        g_lastDecision = fmt::format("{} {}", input.push ? "press" : "release", what);
        g_lastDecisionStep = m_currentStep;
        if (print) log::info("Survival delay: {}", g_lastDecision);
    }

    // Decides what to do with the oldest pending input. Returns false if it
    // has to keep waiting.
    bool resolve(PendingInput& input) {
        auto& dropped = m_fields->m_droppedPush[input.isPlayer2 ? 1 : 0];

        if (!input.push && dropped) {
            dropped = false;
            return true;
        }
        if (input.push) dropped = false;

        int waited = std::max(0, m_currentStep - input.queuedStep);
        if (this->survivesAt(input, 0)) {
            this->applyInput(input);
            if (input.held) this->record(input, fmt::format("held {} steps, then applied", waited), true);
            else this->record(input, "applied (safe)", false);
            return true;
        }
        if (this->waitingHelps(input, g_hacks.delaySteps - waited)) {
            if (!input.held) this->record(input, "would kill you, holding", true);
            input.held = true;
            return false;
        }
        if (input.push && this->survivesAt(input, -1)) {
            dropped = true;
            this->record(input, fmt::format("cancelled after {} steps", waited), true);
            return true;
        }
        this->applyInput(input);
        this->record(input, "deadly at every timing, applied anyway", true);
        return true;
    }

    void update(float dt) {
        auto fields = m_fields.operator->();
        auto& pending = fields->m_pending;
        bool active = g_hacks.survivalDelay && !g_hacks.frameAdvance
            && typeinfo_cast<PlayLayer*>(this)
            && !m_player1->m_isDead && !m_levelEndAnimationStarted;

        if (!active) {
            // Turned off mid-hold: don't lose inputs. Dead: they're stale.
            if (typeinfo_cast<PlayLayer*>(this) && !m_player1->m_isDead) {
                for (auto const& input : pending) this->applyInput(input);
            }
            pending.clear();
            fields->m_droppedPush[0] = fields->m_droppedPush[1] = false;
            return GJBaseGameLayer::update(dt);
        }

        // Take this frame's jump inputs out of GD's queue.
        for (auto it = m_queuedButtons.begin(); it != m_queuedButtons.end();) {
            if (it->m_button == PlayerButton::Jump) {
                pending.push_back({ it->m_isPush, it->m_isPlayer2, m_currentStep });
                it = m_queuedButtons.erase(it);
            }
            else {
                ++it;
            }
        }

        while (!pending.empty() && this->resolve(pending.front())) {
            pending.pop_front();
        }

        GJBaseGameLayer::update(dt);
    }

    // Things the simulated player may touch that would change the real game.
    void playerTouchedRing(PlayerObject* player, RingObject* object) {
        if (isSim(player)) return;
        GJBaseGameLayer::playerTouchedRing(player, object);
    }
    void playerTouchedTrigger(PlayerObject* player, EffectGameObject* object) {
        if (isSim(player)) return;
        GJBaseGameLayer::playerTouchedTrigger(player, object);
    }
    void bumpPlayer(PlayerObject* player, EffectGameObject* object) {
        if (isSim(player)) return;
        GJBaseGameLayer::bumpPlayer(player, object);
    }
    void teleportPlayer(TeleportPortalObject* object, PlayerObject* player) {
        if (isSim(player)) return;
        GJBaseGameLayer::teleportPlayer(object, player);
    }
    void playerWillSwitchMode(PlayerObject* player, GameObject* object) {
        if (isSim(player)) return;
        GJBaseGameLayer::playerWillSwitchMode(player, object);
    }
    void toggleDualMode(GameObject* object, bool dual, PlayerObject* player, bool noEffects) {
        if (isSim(player)) return;
        GJBaseGameLayer::toggleDualMode(object, dual, player, noEffects);
    }
    void updateDualGround(PlayerObject* player, int mode, bool instant, float duration) {
        if (isSim(player)) return;
        GJBaseGameLayer::updateDualGround(player, mode, instant, duration);
    }
    void flipGravity(PlayerObject* player, bool flip, bool noEffects) {
        // Gravity matters for the prediction, so flip the copy, quietly.
        if (isSim(player)) return player->flipGravity(flip, true);
        GJBaseGameLayer::flipGravity(player, flip, noEffects);
    }
    void toggleFlipped(bool flip, bool noEffects) {
        if (g_sim.active) return;
        GJBaseGameLayer::toggleFlipped(flip, noEffects);
    }
    void pickupItem(EffectGameObject* object) {
        if (g_sim.active) return;
        GJBaseGameLayer::pickupItem(object);
    }
    void destroyObject(GameObject* object) {
        if (g_sim.active) return;
        GJBaseGameLayer::destroyObject(object);
    }
    void activateSFXTrigger(SFXTriggerGameObject* object) {
        if (g_sim.active) return;
        GJBaseGameLayer::activateSFXTrigger(object);
    }
    void gameEventTriggered(GJGameEvent event, int material, int playerID) {
        if (g_sim.active) return;
        GJBaseGameLayer::gameEventTriggered(event, material, playerID);
    }
    void lightningFlash(CCPoint from, CCPoint to, ccColor3B color, float lineWidth, float duration, int displacement, bool flash, float opacity) {
        if (g_sim.active) return;
        GJBaseGameLayer::lightningFlash(from, to, color, lineWidth, duration, displacement, flash, opacity);
    }
};

class $modify(SurvivalDelayPlayer, PlayerObject) {
    void update(float dt) {
        if (!g_sim.active) g_stepDelta = dt;
        PlayerObject::update(dt);
    }
    void incrementJumps() {
        if (isSim(this)) return;
        PlayerObject::incrementJumps();
    }
    void playSpiderDashEffect(CCPoint from, CCPoint to) {
        if (isSim(this)) return;
        PlayerObject::playSpiderDashEffect(from, to);
    }
    void spawnPortalCircle(ccColor3B color, float startRadius) {
        if (isSim(this)) return;
        PlayerObject::spawnPortalCircle(color, startRadius);
    }
};

class $modify(SurvivalDelayPlayLayer, PlayLayer) {
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (!g_sim.active && g_hacks.survivalDelay && !g_hacks.noclip
            && object != m_anticheatSpike && player && !player->m_isDead) {
            log::info(
                "Survival delay: died; last input was {} ({} steps before)",
                g_lastDecision, m_currentStep - g_lastDecisionStep
            );
        }
        PlayLayer::destroyPlayer(player, object);
    }

    // The copy reaching the end of the level counts as surviving, and must
    // not actually complete it.
    void levelComplete() {
        if (g_sim.active) return;
        PlayLayer::levelComplete();
    }
    void playEndAnimationToPos(CCPoint position) {
        if (g_sim.active) return;
        PlayLayer::playEndAnimationToPos(position);
    }
    void playPlatformerEndAnimationToPos(CCPoint position, bool instant) {
        if (g_sim.active) return;
        PlayLayer::playPlatformerEndAnimationToPos(position, instant);
    }
};
