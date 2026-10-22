#include <Geode/Geode.hpp>
#include <Geode/modify/CCDisplayLinkDirector.hpp>
#include "State.hpp"

using namespace geode::prelude;

// FPS bypass. GD sets the frame rate with setAnimationInterval (from its own
// FPS option and on some window events). We remember what GD asked for and
// pass our own value instead while the bypass is on. VSync still caps you
// at your monitor's refresh rate, so turn it off in GD's settings.
namespace {
    double g_gameInterval = 1.0 / 60.0;
    bool g_applying = false;

    void apply() {
        g_applying = true;
        CCDirector::get()->setAnimationInterval(g_gameInterval);
        g_applying = false;
    }
}

class $modify(FpsBypassDirector, CCDisplayLinkDirector) {
    void setAnimationInterval(double interval) {
        if (!g_applying) g_gameInterval = interval;
        if (g_hacks.fpsBypass && g_hacks.fps > 0.f) {
            interval = 1.0 / g_hacks.fps;
        }
        CCDisplayLinkDirector::setAnimationInterval(interval);
    }
};

$on_mod(Loaded) {
    g_gameInterval = CCDirector::get()->getAnimationInterval();

    g_hacks.fpsBypass = Mod::get()->getSettingValue<bool>("fps-bypass");
    g_hacks.fps = static_cast<float>(Mod::get()->getSettingValue<double>("fps"));
    listenForSettingChanges<bool>("fps-bypass", [](bool v) {
        g_hacks.fpsBypass = v;
        apply();
    });
    listenForSettingChanges<double>("fps", [](double v) {
        g_hacks.fps = static_cast<float>(v);
        apply();
    });
    if (g_hacks.fpsBypass) apply();
}
