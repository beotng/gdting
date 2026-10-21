#include "HackMenu.hpp"
#include "State.hpp"

#include <algorithm>
#include <cstdlib>

using namespace geode::prelude;

namespace {
    struct ToggleDef {
        char const* key;
        char const* label;
        bool HackState::* field;
    };

    constexpr ToggleDef TOGGLES[] = {
        { "noclip", "Noclip", &HackState::noclip },
        { "speedhack", "Speedhack", &HackState::speedhack },
        { "speedhack-audio", "Speedhack audio", &HackState::speedhackAudio },
        { "hitboxes", "Show hitboxes", &HackState::hitboxes },
    };

    constexpr float WIDTH = 280.f;
    constexpr float HEIGHT = 250.f;
}

HackMenu* HackMenu::create() {
    auto ret = new HackMenu();
    if (ret->initMenu()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

void HackMenu::open() {
    auto scene = CCScene::get();
    if (!scene || scene->getChildByID("hack-menu"_spr)) return;

    if (auto menu = HackMenu::create()) {
        menu->setID("hack-menu"_spr);
        menu->show();
    }
}

bool HackMenu::initMenu() {
    if (!Popup::init(WIDTH, HEIGHT)) return false;
    this->setTitle("Hack Menu");

    auto menu = CCMenu::create();
    menu->setPosition({ 0.f, 0.f });
    menu->setContentSize({ WIDTH, HEIGHT });
    m_mainLayer->addChild(menu);

    float y = HEIGHT - 60.f;
    for (int i = 0; i < static_cast<int>(std::size(TOGGLES)); i++) {
        this->addToggle(menu, i, y);
        y -= 35.f;
    }

    auto speedLabel = CCLabelBMFont::create("Speed", "bigFont.fnt");
    speedLabel->setScale(.5f);
    speedLabel->setAnchorPoint({ 0.f, .5f });
    speedLabel->setPosition({ 30.f, y });
    m_mainLayer->addChild(speedLabel);

    auto speedInput = TextInput::create(90.f, "1.00");
    speedInput->setCommonFilter(CommonFilter::Float);
    speedInput->setString(fmt::format("{:.2f}", g_hacks.speed));
    speedInput->setPosition({ WIDTH - 75.f, y });
    speedInput->setCallback([](std::string const& text) {
        char* end = nullptr;
        double value = std::strtod(text.c_str(), &end);
        if (end == text.c_str()) return;
        Mod::get()->setSettingValue<double>("speed", std::clamp(value, 0.1, 10.0));
    });
    m_mainLayer->addChild(speedInput);

    return true;
}

void HackMenu::addToggle(CCMenu* menu, int index, float y) {
    auto const& def = TOGGLES[index];

    auto label = CCLabelBMFont::create(def.label, "bigFont.fnt");
    label->setScale(.5f);
    label->setAnchorPoint({ 0.f, .5f });
    label->setPosition({ 30.f, y });
    m_mainLayer->addChild(label);

    auto toggler = CCMenuItemToggler::createWithStandardSprites(
        this, menu_selector(HackMenu::onToggle), .7f
    );
    toggler->toggle(g_hacks.*def.field);
    toggler->setTag(index);
    toggler->setPosition({ WIDTH - 45.f, y });
    menu->addChild(toggler);
}

void HackMenu::onToggle(CCObject* sender) {
    auto toggler = static_cast<CCMenuItemToggler*>(sender);
    auto const& def = TOGGLES[toggler->getTag()];
    // The toggler flips its own state after this callback runs, so the new
    // value is the opposite of what it shows right now.
    Mod::get()->setSettingValue<bool>(def.key, !toggler->isToggled());
}
