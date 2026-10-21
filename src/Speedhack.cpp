#include <Geode/Geode.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/fmod/fmod.hpp>
#include "State.hpp"

using namespace geode::prelude;

namespace {
    float g_appliedPitch = 1.f;

    void setAudioPitch(float pitch) {
        auto engine = FMODAudioEngine::sharedEngine();
        if (!engine || !engine->m_system) return;

        FMOD::ChannelGroup* master = nullptr;
        if (engine->m_system->getMasterChannelGroup(&master) == FMOD_OK && master) {
            master->setPitch(pitch);
            g_appliedPitch = pitch;
        }
    }
}

// Every scheduled update in the game (gameplay, animations, actions) gets its
// delta time from CCScheduler::update, so scaling dt here scales everything.
class $modify(SpeedhackScheduler, CCScheduler) {
    void update(float dt) {
        float speed = g_hacks.speedhack ? g_hacks.speed : 1.f;

        float pitch = g_hacks.speedhackAudio ? speed : 1.f;
        if (pitch != g_appliedPitch) {
            setAudioPitch(pitch);
        }

        CCScheduler::update(dt * speed);
    }
};
