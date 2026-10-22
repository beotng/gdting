#pragma once

class PlayerObject;

// Set while SurvivalDelay is stepping its invisible copy of the player ahead.
// Other hooks check it so the copy can't kill the real player, finish the
// level, or change anything else in the real game.
struct SimulationState {
    bool active = false;
    bool died = false;
    PlayerObject* player = nullptr;
};

inline SimulationState g_sim;
