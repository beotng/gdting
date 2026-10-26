#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include "Assist.hpp"
#include "Simulation.hpp"
#include "State.hpp"

using namespace geode::prelude;

// Assist HUD: top-left label showing the last time click buffer, survival
// delay or coyote time changed an input, and by how much, e.g.
// "Delay +6t" (held 6 physics steps) or "Coyote -3t" (3 steps late). It
// fades out a second after each use.
namespace {
    constexpr float SHOW_SECONDS = 1.f;
}

class $modify(AssistHudPlayLayer, PlayLayer) {
    struct Fields {
        CCLabelBMFont* m_label = nullptr;
        int m_shownSerial = 0;
        float m_shownFor = SHOW_SECONDS;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        auto winSize = CCDirector::get()->getWinSize();
        auto label = CCLabelBMFont::create("", "bigFont.fnt");
        label->setScale(.35f);
        label->setAnchorPoint({ 0.f, 1.f });
        label->setPosition({ 6.f, winSize.height - 6.f });
        label->setID("assist-hud"_spr);
        m_uiLayer->addChild(label);
        m_fields->m_label = label;
        m_fields->m_shownSerial = g_assist.serial;
        return true;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        auto fields = m_fields.operator->();
        if (!fields->m_label || g_sim.active) return;

        if (g_assist.serial != fields->m_shownSerial && g_assist.what) {
            fields->m_shownSerial = g_assist.serial;
            fields->m_shownFor = 0.f;
            bool late = std::string_view(g_assist.what) == "Coyote";
            fields->m_label->setString(
                fmt::format("{} {}{}t", g_assist.what, late ? "-" : "+", g_assist.steps).c_str()
            );
        }

        fields->m_shownFor += dt;
        float left = 1.f - fields->m_shownFor / SHOW_SECONDS;
        fields->m_label->setVisible(g_hacks.assistHud && left > 0.f);
        fields->m_label->setOpacity(static_cast<GLubyte>(255 * std::clamp(left, 0.f, 1.f)));
    }
};
