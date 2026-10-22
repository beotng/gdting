#pragma once

#include <Geode/Geode.hpp>

class HackMenu : public geode::Popup {
protected:
    bool initMenu();
    void addToggle(cocos2d::CCMenu* menu, int index, cocos2d::CCPoint origin);
    void addNumberInput(
        char const* key, char const* label, double min, double max, bool isInt, cocos2d::CCPoint origin
    );
    void onToggle(cocos2d::CCObject* sender);

public:
    static HackMenu* create();
    // Opens the menu unless it's already on screen.
    static void open();
};
