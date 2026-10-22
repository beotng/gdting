#include <Geode/Geode.hpp>
#include "State.hpp"

using namespace geode::prelude;

namespace {
    template <class T, class F>
    void bind(char const* key, F apply) {
        apply(Mod::get()->getSettingValue<T>(key));
        listenForSettingChanges<T>(key, apply);
    }
}

// Load the saved settings into g_hacks and keep it in sync whenever a
// setting changes, whether from our menu or from Geode's settings page.
$on_mod(Loaded) {
    bind<bool>("noclip", [](bool v) { g_hacks.noclip = v; });
    bind<bool>("speedhack", [](bool v) { g_hacks.speedhack = v; });
    bind<double>("speed", [](double v) { g_hacks.speed = static_cast<float>(v); });
    bind<bool>("speedhack-audio", [](bool v) { g_hacks.speedhackAudio = v; });
    bind<bool>("hitboxes", [](bool v) { g_hacks.hitboxes = v; });
    bind<bool>("click-buffer", [](bool v) { g_hacks.clickBuffer = v; });
    bind<int64_t>("buffer-steps", [](int64_t v) { g_hacks.bufferSteps = static_cast<int>(v); });
    bind<bool>("frame-advance", [](bool v) { g_hacks.frameAdvance = v; });
    bind<int64_t>("advance-steps", [](int64_t v) { g_hacks.advanceSteps = static_cast<int>(v); });
    bind<bool>("safe-mode", [](bool v) { g_hacks.safeMode = v; });
    bind<bool>("survival-delay", [](bool v) { g_hacks.survivalDelay = v; });
    bind<int64_t>("delay-steps", [](int64_t v) { g_hacks.delaySteps = static_cast<int>(v); });
    bind<int64_t>("max-lookahead", [](int64_t v) { g_hacks.maxLookahead = static_cast<int>(v); });
    bind<bool>("cheat-indicator", [](bool v) { g_hacks.cheatIndicator = v; });
    // fps-bypass and fps are bound in FpsBypass.cpp, since changing them
    // has to re-apply the frame rate.
}
