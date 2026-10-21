#include <Geode/Geode.hpp>
#include "State.hpp"

using namespace geode::prelude;

// Load the saved settings into g_hacks and keep it in sync whenever a
// setting changes, whether from our menu or from Geode's settings page.
$on_mod(Loaded) {
    auto mod = Mod::get();
    g_hacks.noclip = mod->getSettingValue<bool>("noclip");
    g_hacks.speedhack = mod->getSettingValue<bool>("speedhack");
    g_hacks.speed = static_cast<float>(mod->getSettingValue<double>("speed"));
    g_hacks.speedhackAudio = mod->getSettingValue<bool>("speedhack-audio");
    g_hacks.hitboxes = mod->getSettingValue<bool>("hitboxes");

    listenForSettingChanges<bool>("noclip", [](bool v) { g_hacks.noclip = v; });
    listenForSettingChanges<bool>("speedhack", [](bool v) { g_hacks.speedhack = v; });
    listenForSettingChanges<double>("speed", [](double v) { g_hacks.speed = static_cast<float>(v); });
    listenForSettingChanges<bool>("speedhack-audio", [](bool v) { g_hacks.speedhackAudio = v; });
    listenForSettingChanges<bool>("hitboxes", [](bool v) { g_hacks.hitboxes = v; });
}
