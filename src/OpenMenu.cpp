#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include "FrameAdvance.hpp"
#include "HackMenu.hpp"
#include "State.hpp"

using namespace geode::prelude;

// Adds a "Hacks" button to the pause menu.
class $modify(HackPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto menu = this->getChildByID("right-button-menu");
        if (!menu) {
            log::warn("right-button-menu not found, skipping pause button");
            return;
        }

        auto sprite = ButtonSprite::create("Hacks");
        sprite->setScale(.6f);
        auto button = CCMenuItemSpriteExtra::create(
            sprite, this, menu_selector(HackPauseLayer::onHackMenu)
        );
        button->setID("hack-menu-button"_spr);
        menu->addChild(button);
        menu->updateLayout();
    }

    void onHackMenu(CCObject*) {
        HackMenu::open();
    }
};

namespace {
    void toggleSetting(char const* key, char const* name) {
        bool value = !Mod::get()->getSettingValue<bool>(key);
        Mod::get()->setSettingValue<bool>(key, value);
        Notification::create(
            fmt::format("{}: {}", name, value ? "on" : "off"), NotificationIcon::Info, .8f
        )->show();
    }
}

// Hotkeys:
//   Tab  opens the menu from anywhere
//   F6   toggles frame advance
//   F7   steps forward while frame advance is on (hold to repeat)
//   F8   toggles the click buffer
//   F9   toggles survival delay
class $modify(CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool isKeyDown, bool isKeyRepeat, double timestamp) {
        if (isKeyDown) {
            switch (key) {
                case KEY_Tab:
                    if (isKeyRepeat) break;
                    HackMenu::open();
                    return true;
                case KEY_F6:
                    if (isKeyRepeat) break;
                    toggleSetting("frame-advance", "Frame advance");
                    return true;
                case KEY_F7:
                    if (!g_hacks.frameAdvance) break;
                    frame_advance::step(g_hacks.advanceSteps);
                    return true;
                case KEY_F8:
                    if (isKeyRepeat) break;
                    toggleSetting("click-buffer", "Click buffer");
                    return true;
                case KEY_F9:
                    if (isKeyRepeat) break;
                    toggleSetting("survival-delay", "Survival delay");
                    return true;
                default:
                    break;
            }
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, isKeyDown, isKeyRepeat, timestamp);
    }
};
