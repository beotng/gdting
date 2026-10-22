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
// queue and checked before it's applied: like a trajectory preview, an
// invisible copy of the player is stepped ahead against the level's hitboxes
// with the input applied now, a little later, or never, following the
// input's own trajectory (until a jump lands; briefly for flying modes).
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
    // Real physics steps taken by player 1, used to time inputs.
    int g_realStep = 0;

    // How far an input's trajectory is followed. Ground modes: until the
    // player has landed and stayed down a moment. Flying modes: a short
    // window, since the next input comes soon anyway.
    constexpr int MIN_AIR_STEPS = 6;
    constexpr int LANDED_STEPS = 8;
    constexpr int FLYING_HORIZON = 48;

    struct PendingInput {
        bool push;
        bool isPlayer2;
        int queuedStep;
        bool held = false;
    };

    struct SimResult {
        bool died = false;
        // Steps simulated, or the step the copy died on.
        int steps = 0;
    };

    // What was predicted for the last input that went through, to compare
    // with reality if the player then dies.
    struct Prediction {
        std::string what = "none";
        int step = 0;
        int horizon = 0;
        bool safe = false;
    };
    Prediction g_last;

    bool isSim(PlayerObject* player) {
        return g_sim.active && player == g_sim.player;
    }

    bool isFlying(PlayerObject* player) {
        return player->m_isShip || player->m_isBird || player->m_isDart || player->m_isSwing;
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

    // Steps a copy of `real` ahead like a trajectory preview, applying a jump
    // input after `applyAt` steps (never if negative). Follows the input's
    // trajectory (see MIN_AIR_STEPS etc.), up to `max-lookahead` steps, or
    // exactly `fixedSteps` steps if that's given.
    SimResult simulate(PlayerObject* real, int applyAt, bool push, int fixedSteps) {
        if (!real || real->m_isDead) return {};

        auto sim = this->getSimPlayer();
        sim->copyAttributes(real);
        sim->setPosition(real->getPosition());
        sim->m_holdingButtons = real->m_holdingButtons;
        sim->m_jumpBuffered = real->m_jumpBuffered;
        sim->m_isDead = false;

        g_sim = { true, false, sim };
        int start = std::max(applyAt, 0);
        int cap = fixedSteps >= 0 ? fixedSteps : start + g_hacks.maxLookahead;
        int steps = 0;
        int grounded = 0;
        while (steps < cap && !g_sim.died) {
            if (steps == applyAt) {
                if (push) sim->pushButton(PlayerButton::Jump);
                else sim->releaseButton(PlayerButton::Jump);
            }
            sim->update(g_stepDelta);
            this->checkCollisions(sim, g_stepDelta, false);
            sim->updateRotation(g_stepDelta);
            steps++;

            if (fixedSteps >= 0 || g_sim.died) continue;
            int since = steps - start;
            if (isFlying(sim)) {
                if (since >= FLYING_HORIZON) break;
            }
            else if (since >= MIN_AIR_STEPS) {
                grounded = sim->m_isOnGround ? grounded + 1 : 0;
                if (grounded >= LANDED_STEPS) break;
            }
        }
        SimResult result = { g_sim.died, steps };
        g_sim = {};
        return result;
    }

    // Which real players a jump input moves.
    std::pair<bool, bool> affectedPlayers(PendingInput const& input) {
        bool dual = m_gameState.m_isDualMode && m_player2;
        bool twoPlayer = m_levelSettings && m_levelSettings->m_twoPlayerMode;
        bool p1 = !twoPlayer || !input.isPlayer2;
        bool p2 = dual && (!twoPlayer || input.isPlayer2);
        return { p1, p2 };
    }

    // Simulates every player the input moves; dies if any of them does.
    SimResult simulateInput(PendingInput const& input, int applyAt, int fixedSteps = -1) {
        auto [p1, p2] = this->affectedPlayers(input);
        SimResult result;
        for (auto [moves, player] : { std::pair{ p1, m_player1 }, std::pair{ p2, m_player2 } }) {
            if (!moves) continue;
            auto r = this->simulate(player, applyAt, input.push, fixedSteps);
            if (r.died) return r;
            result.steps = std::max(result.steps, r.steps);
        }
        return result;
    }

    // The first of a few evenly spaced later timings (within `remaining`
    // steps) at which the input is safe, or -1 if none is.
    int safeLaterTiming(PendingInput const& input, int remaining) {
        if (remaining <= 0) return -1;
        int stride = std::max(1, remaining / 6);
        for (int k = stride; k <= remaining; k += stride) {
            if (!this->simulateInput(input, k).died) return k;
        }
        return -1;
    }

    void applyInput(PendingInput const& input) {
        this->handleButton(input.push, static_cast<int>(PlayerButton::Jump), !input.isPlayer2);
    }

    void report(PendingInput const& input, std::string const& what, int horizon, bool safe) {
        g_last = { fmt::format("{} {}", input.push ? "press" : "release", what), g_realStep, horizon, safe };
        log::info("Survival delay: {}", g_last.what);
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

        int waited = std::max(0, g_realStep - input.queuedStep);
        auto now = this->simulateInput(input, 0);
        if (!now.died) {
            this->applyInput(input);
            this->report(input, input.held
                ? fmt::format("held {} steps, then safe (checked {} steps)", waited, now.steps)
                : fmt::format("safe (checked {} steps)", now.steps), now.steps, true);
            return true;
        }

        int later = this->safeLaterTiming(input, g_hacks.delaySteps - waited);
        if (later >= 0) {
            if (!input.held) {
                log::info(
                    "Survival delay: {} now dies in {} steps, safe {} steps later; holding",
                    input.push ? "press" : "release", now.steps, later
                );
            }
            input.held = true;
            return false;
        }

        auto never = this->simulateInput(input, -1, now.steps);
        if (input.push && !never.died) {
            dropped = true;
            this->report(input, fmt::format(
                "dies in {} steps, not jumping survives; cancelled after {} steps", now.steps, waited
            ), now.steps, true);
            return true;
        }
        this->applyInput(input);
        this->report(input, fmt::format(
            "dies in {} steps, not pressing dies in {}; applied anyway", now.steps, never.steps
        ), now.steps, false);
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
                pending.push_back({ it->m_isPush, it->m_isPlayer2, g_realStep });
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
        if (!g_sim.active) {
            g_stepDelta = dt;
            auto playLayer = PlayLayer::get();
            if (playLayer && this == playLayer->m_player1) g_realStep++;
        }
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
            int since = g_realStep - g_last.step;
            log::info("Survival delay: died {} steps after last input ({})", since, g_last.what);
            if (g_last.safe && since < g_last.horizon) {
                log::warn(
                    "Survival delay: MISPREDICTED - copy survived {} steps, real player died after {}",
                    g_last.horizon, since
                );
            }
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
