#pragma once

// Cached copies of the mod settings, so hooks that run every frame
// don't have to look settings up by name.
struct HackState {
    bool noclip = false;
    bool speedhack = false;
    float speed = 1.f;
    bool speedhackAudio = true;
    bool hitboxes = false;
};

inline HackState g_hacks;
