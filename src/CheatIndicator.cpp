#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include "State.hpp"

using namespace geode::prelude;

// A small label in the bottom-left corner while playing: red "Cheating" when
// a cheat is on, yellow "Safe mode" when progress isn't being saved.
class $modify(CheatIndicatorPlayLayer, PlayLayer) {
    struct Fields {
        CCLabelBMFont* m_label = nullptr;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        auto label = CCLabelBMFont::create("", "bigFont.fnt");
        label->setScale(.35f);
        label->setOpacity(160);
        label->setAnchorPoint({ 0.f, 0.f });
        label->setPosition({ 4.f, 4.f });
        label->setID("cheat-indicator"_spr);
        this->addChild(label, 1000);
        m_fields->m_label = label;
        return true;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        auto label = m_fields->m_label;
        if (!label) return;

        label->setVisible(g_hacks.cheatIndicator);
        if (!g_hacks.cheatIndicator) return;

        char const* text = "";
        ccColor3B color = { 255, 255, 255 };
        if (g_hacks.anyCheats()) {
            text = "Cheating";
            color = { 255, 80, 80 };
        }
        else if (g_hacks.safeModeActive()) {
            text = "Safe mode";
            color = { 255, 220, 80 };
        }
        // setString rebuilds the label, so only call it when the text changes.
        if (std::string_view(label->getString()) != text) {
            label->setString(text);
        }
        label->setColor(color);
    }
};
