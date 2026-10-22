#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include "Simulation.hpp"
#include "State.hpp"

#include <deque>

using namespace geode::prelude;

// Survival delay. Every jump press or release is taken out of GD's input
// queue and checked before it's applied: an invisible copy of the player is
// stepped `delay-lookahead` physics steps ahead against the level's hitboxes,
// once with the input and once without.
//
//  - Safe input: applied right away.
//  - Input that kills you but waiting doesn't: held, and re-checked every
//    frame for up to `delay-frames` frames.
//  - Still deadly after that: a press is cancelled (along with its release);
//    a release goes through, so you never get stuck holding.
//  - Input that kills you either way: applied, since waiting can't help.
//
// The real game is never stepped or rolled back, so nothing in the level can
// get out of sync. The copy only collides with the level; anything it touches
// that would change the real game (triggers, coins, portals that move the
// camera, end of level) is ignored while simulating.
namespace {
    // GD's real per-step delta, recorded from the real player each step.
    float g_stepDelta = 0.25f;

    struct PendingInput {
        bool push;
        bool isPlayer2;
        int age = 0;
    };

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

    // Steps a copy of `real` ahead, optionally applying a jump input first.
    // Returns whether it's still alive at the end.
    bool simulate(PlayerObject* real, bool applyInput, bool push) {
        if (!real || real->m_isDead) return true;

        auto sim = this->getSimPlayer();
        sim->copyAttributes(real);
        sim->setPosition(real->getPosition());
        sim->m_holdingButtons = real->m_holdingButtons;
        sim->m_jumpBuffered = real->m_jumpBuffered;
        sim->m_isDead = false;

        g_sim = { true, false, sim };
        if (applyInput) {
            if (push) sim->pushButton(PlayerButton::Jump);
            else sim->releaseButton(PlayerButton::Jump);
        }
        for (int i = 0; i < g_hacks.delayLookahead && !g_sim.died; i++) {
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

    bool survives(PendingInput const& input, bool applyInput) {
        auto [p1, p2] = this->affectedPlayers(input);
        if (p1 && !this->simulate(m_player1, applyInput, input.push)) return false;
        if (p2 && !this->simulate(m_player2, applyInput, input.push)) return false;
        return true;
    }

    void applyInput(PendingInput const& input) {
        this->handleButton(input.push, static_cast<int>(PlayerButton::Jump), !input.isPlayer2);
    }

    // Decides what to do with the oldest pending input. Returns false if it
    // has to keep waiting.
    bool resolve(PendingInput const& input) {
        auto& dropped = m_fields->m_droppedPush[input.isPlayer2 ? 1 : 0];

        if (!input.push && dropped) {
            dropped = false;
            return true;
        }
        if (input.push) dropped = false;

        if (this->survives(input, true) || !this->survives(input, false)) {
            this->applyInput(input);
            return true;
        }
        if (input.age < g_hacks.delayFrames) return false;

        if (input.push) dropped = true;
        else this->applyInput(input);
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
                pending.push_back({ it->m_isPush, it->m_isPlayer2 });
                it = m_queuedButtons.erase(it);
            }
            else {
                ++it;
            }
        }

        while (!pending.empty() && this->resolve(pending.front())) {
            pending.pop_front();
        }
        for (auto& input : pending) input.age++;

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
