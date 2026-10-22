#pragma once

namespace frame_advance {
    // Queue `steps` physics steps to run on the next frame while frozen.
    void step(int steps);
}
