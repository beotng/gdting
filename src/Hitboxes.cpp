#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include "State.hpp"

using namespace geode::prelude;

// GD already has a hitbox renderer (GJBaseGameLayer::updateDebugDraw), but
// only runs it in practice mode with "Show Hitboxes" on. We run it ourselves
// after every frame so it works in normal mode too.
class $modify(HitboxPlayLayer, PlayLayer) {
    struct Fields {
        bool m_drawing = false;
    };

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (!m_debugDrawNode) return;

        if (g_hacks.hitboxes) {
            bool wasEnabled = m_isDebugDrawEnabled;
            m_isDebugDrawEnabled = true;
            m_debugDrawNode->setVisible(true);
            this->updateDebugDraw();
            m_isDebugDrawEnabled = wasEnabled;
            m_fields->m_drawing = true;
        }
        else if (m_fields->m_drawing) {
            // Turned off this frame: clear what we drew, unless GD's own
            // practice-mode hitboxes are meant to stay on.
            m_fields->m_drawing = false;
            if (!m_isDebugDrawEnabled) {
                m_debugDrawNode->clear();
                m_debugDrawNode->setVisible(false);
            }
        }
    }
};
