#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include "HackMenu.hpp"

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

// Tab opens the menu from anywhere.
class $modify(CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool isKeyDown, bool isKeyRepeat, double timestamp) {
        if (key == KEY_Tab && isKeyDown && !isKeyRepeat) {
            HackMenu::open();
            return true;
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, isKeyDown, isKeyRepeat, timestamp);
    }
};
