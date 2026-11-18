#pragma once
// Pen and pointer input that GLFW doesn't pass on (macOS).
//
// 1. A pen tap (Apple Pencil through Sidecar, drawing tablets) arrives without a mouse
//    move first. GLFW only updates the pointer position on moves, so the click would land
//    where the pointer was before. A Cocoa event monitor sends the true position to ImGui
//    just before each click, and marks pen events as pen input.
// 2. Pen pressure and tilt, kept for later tools and shown in Help -> Pen and mouse test.
// Elsewhere these are no-ops.
#include <cstdint>

namespace mazz {

struct PenState {
    bool pen = false;          // last pointer event came from a pen
    float pressure = 0;        // 0..1 (pen only)
    float tiltX = 0, tiltY = 0; // -1..1 (pen only)
    float x = 0, y = 0;        // last position sent to ImGui (ImGui coordinates)
    const char* lastEvent = "-";
    uint64_t events = 0;       // pointer events seen
    uint64_t positionFixes = 0; // clicks whose position had to be supplied
};

void installPenInput();
const PenState& penState();

} // namespace mazz
