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
        { "hitboxes", "Show hitboxes", &HackState::hitboxes },
        { "speedhack", "Speedhack", &HackState::speedhack },
        { "speedhack-audio", "Speedhack audio", &HackState::speedhackAudio },
        { "click-buffer", "Click buffer", &HackState::clickBuffer },
        { "survival-delay", "Survival delay", &HackState::survivalDelay },
        { "show-trajectory", "Show trajectory", &HackState::showTrajectory },
        { "frame-advance", "Frame advance", &HackState::frameAdvance },
        { "fps-bypass", "FPS bypass", &HackState::fpsBypass },
        { "safe-mode", "Safe mode", &HackState::safeMode },
        { "cheat-indicator", "Cheat indicator", &HackState::cheatIndicator },
    };

    struct NumberDef {
        char const* key;
        char const* label;
        double min;
        double max;
        bool isInt;
    };

    constexpr NumberDef NUMBERS[] = {
        { "speed", "Speed", .1, 10., false },
        { "fps", "FPS", 30., 1000., false },
        { "buffer-steps", "Buffer steps", 1., 60., true },
        { "advance-steps", "Advance steps", 1., 60., true },
        { "delay-steps", "Max delay", 1., 60., true },
        { "max-lookahead", "Max lookahead", 60., 480., true },
    };

    constexpr float WIDTH = 420.f;
    constexpr float HEIGHT = 320.f;
    constexpr float COLUMN_WIDTH = WIDTH / 2.f;
    constexpr float ROW_HEIGHT = 27.f;
    constexpr float TOP = HEIGHT - 55.f;

    // Toggles and inputs are laid out left to right, two per row.
    CCPoint cellOrigin(int index) {
        return { (index % 2) * COLUMN_WIDTH, TOP - (index / 2) * ROW_HEIGHT };
    }

    void addLabel(CCNode* parent, char const* text, CCPoint pos) {
        auto label = CCLabelBMFont::create(text, "bigFont.fnt");
        label->setScale(.4f);
        label->setAnchorPoint({ 0.f, .5f });
        label->setPosition(pos);
        parent->addChild(label);
    }
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

    int cell = 0;
    for (int i = 0; i < static_cast<int>(std::size(TOGGLES)); i++) {
        this->addToggle(menu, i, cellOrigin(cell++));
    }
    // Number inputs start on a fresh row.
    cell += cell % 2;
    for (auto const& def : NUMBERS) {
        this->addNumberInput(def.key, def.label, def.min, def.max, def.isInt, cellOrigin(cell++));
    }

    auto hint = CCLabelBMFont::create(
        "F2: menu   F6: frame advance   F7: step   F8: click buffer   F9: survival delay", "chatFont.fnt"
    );
    hint->setScale(.6f);
    hint->setOpacity(180);
    hint->setPosition({ WIDTH / 2.f, 16.f });
    m_mainLayer->addChild(hint);

    return true;
}

void HackMenu::addToggle(CCMenu* menu, int index, CCPoint origin) {
    auto const& def = TOGGLES[index];

    addLabel(m_mainLayer, def.label, { origin.x + 20.f, origin.y });

    auto toggler = CCMenuItemToggler::createWithStandardSprites(
        this, menu_selector(HackMenu::onToggle), .6f
    );
    toggler->toggle(g_hacks.*def.field);
    toggler->setTag(index);
    toggler->setPosition({ origin.x + COLUMN_WIDTH - 30.f, origin.y });
    menu->addChild(toggler);
}

void HackMenu::addNumberInput(
    char const* key, char const* label, double min, double max, bool isInt, CCPoint origin
) {
    addLabel(m_mainLayer, label, { origin.x + 20.f, origin.y });

    auto input = TextInput::create(60.f, isInt ? "1" : "1.00");
    input->setCommonFilter(isInt ? CommonFilter::Uint : CommonFilter::Float);
    input->setScale(.8f);
    input->setPosition({ origin.x + COLUMN_WIDTH - 45.f, origin.y });
    if (isInt) {
        input->setString(std::to_string(Mod::get()->getSettingValue<int64_t>(key)));
    }
    else {
        input->setString(fmt::format("{:.2f}", Mod::get()->getSettingValue<double>(key)));
    }
    input->setCallback([key, min, max, isInt](std::string const& text) {
        char* end = nullptr;
        double value = std::strtod(text.c_str(), &end);
        if (end == text.c_str()) return;
        value = std::clamp(value, min, max);
        if (isInt) {
            Mod::get()->setSettingValue<int64_t>(key, static_cast<int64_t>(value));
        }
        else {
            Mod::get()->setSettingValue<double>(key, value);
        }
    });
    m_mainLayer->addChild(input);
}

void HackMenu::onToggle(CCObject* sender) {
    auto toggler = static_cast<CCMenuItemToggler*>(sender);
    auto const& def = TOGGLES[toggler->getTag()];
    // The toggler flips its own state after this callback runs, so the new
    // value is the opposite of what it shows right now.
    Mod::get()->setSettingValue<bool>(def.key, !toggler->isToggled());
}
