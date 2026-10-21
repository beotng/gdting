#pragma once

#include <Geode/Geode.hpp>

class HackMenu : public geode::Popup {
protected:
    bool initMenu();
    void addToggle(cocos2d::CCMenu* menu, int index, float y);
    void onToggle(cocos2d::CCObject* sender);

public:
    static HackMenu* create();
    // Opens the menu unless it's already on screen.
    static void open();
};
