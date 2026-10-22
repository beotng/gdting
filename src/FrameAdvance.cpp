#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/fmod/fmod.hpp>
#include "FrameAdvance.hpp"
#include "Simulation.hpp"
#include "State.hpp"

#include <utility>

using namespace geode::prelude;

// Frame advance / frameskip. While it's on, gameplay is frozen. Each press
// of the step key (F7) runs `advance-steps` physics steps and freezes again,
// so you can go through a part one tick at a time, or skip ahead a few.
// Music is paused while frozen, since it can't follow single steps.
namespace {
    constexpr float STEP = 1.f / 240.f;

    int g_pendingSteps = 0;
    bool g_musicPaused = false;

    void setMusicPaused(bool paused) {
        if (paused == g_musicPaused) return;
        auto engine = FMODAudioEngine::sharedEngine();
        if (!engine || !engine->m_system) return;

        FMOD::ChannelGroup* master = nullptr;
        if (engine->m_system->getMasterChannelGroup(&master) == FMOD_OK && master) {
            master->setPaused(paused);
            g_musicPaused = paused;
        }
    }
}

void frame_advance::step(int steps) {
    g_pendingSteps += steps;
}

class $modify(FrameAdvanceLayer, GJBaseGameLayer) {
    void update(float dt) {
        if (g_sim.active) return GJBaseGameLayer::update(dt);
        bool frozen = g_hacks.frameAdvance && typeinfo_cast<PlayLayer*>(this)
            && !m_levelEndAnimationStarted;
        setMusicPaused(frozen);
        if (!frozen) {
            g_pendingSteps = 0;
            return GJBaseGameLayer::update(dt);
        }

        int steps = std::exchange(g_pendingSteps, 0);
        for (int i = 0; i < steps; i++) {
            // Clear the leftover time GD carries between frames, so each
            // call runs exactly one 240 Hz step (scaled by time warp).
            m_extraDelta = 0.0;
            GJBaseGameLayer::update(STEP * m_gameState.m_timeWarp);
        }
    }
};

class $modify(FrameAdvancePlayLayer, PlayLayer) {
    void onExit() {
        setMusicPaused(false);
        PlayLayer::onExit();
    }
};
