#pragma once

// Set while SurvivalDelay is simulating ahead. Other hooks check it so the
// simulation doesn't kill the player for real, finish the level, or get
// intercepted by other hacks.
struct SimulationState {
    bool active = false;
    bool died = false;
};

inline SimulationState g_sim;
