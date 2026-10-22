#pragma once

// Cached copies of the mod settings, so hooks that run every frame
// don't have to look settings up by name.
struct HackState {
    bool noclip = false;
    bool speedhack = false;
    float speed = 1.f;
    bool speedhackAudio = true;
    bool hitboxes = false;
    bool clickBuffer = false;
    int bufferSteps = 15;
    bool frameAdvance = false;
    int advanceSteps = 1;
    bool fpsBypass = false;
    float fps = 240.f;
    bool safeMode = false;
    bool cheatIndicator = true;
    bool survivalDelay = false;
    int delayFrames = 15;
    int delayLookahead = 120;

    // Hacks GD's own anticheat can't see. While any of these is on, safe
    // mode is forced so the attempt can't save progress or a new best.
    bool undetectedCheats() const {
        return clickBuffer || survivalDelay || frameAdvance || (speedhack && speed != 1.f);
    }

    bool safeModeActive() const {
        return safeMode || undetectedCheats();
    }

    bool anyCheats() const {
        return noclip || undetectedCheats();
    }
};

inline HackState g_hacks;
