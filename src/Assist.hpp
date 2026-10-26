#pragma once

// The last time an input assist changed one of your inputs, for the assist
// HUD: which assist, and by how many physics steps.
struct AssistUse {
    char const* what = nullptr;
    int steps = 0;
    // Bumped on every use, so the HUD can tell a new use from an old one.
    int serial = 0;
};

inline AssistUse g_assist;

inline void reportAssist(char const* what, int steps) {
    g_assist = { what, steps, g_assist.serial + 1 };
}
