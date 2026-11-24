#pragma once
// Every command with a shortcut is an Action: it appears in the menus, in Help ->
// Controls, and keys and scripts trigger it through the same path.
// Keys are matched by physical position (GLFW key codes are US-layout positions), so
// the grid keys right of P are "è" and "+" on an Italian keyboard.
#include <string>

namespace mazz {

enum class Act {
    // File
    New, Open, Save, SaveAs, ExportGlb, ExportGltf, ExportObj, ExportAgain, Quit,
    // Edit
    Undo, Redo, Duplicate, Delete, SelectAll, SelectNone,
    // Select
    ModeVertex, ModeEdge, ModeFace, ModePiece, SelectGrow, SelectShrink, SelectLoop, SelectRing, Hide, HideOthers, ShowAll,
    // Tools
    ToolSelect, ToolVertex, ToolPrimitives, ToolDraw, ToolPushPull, ToolExtrude, ToolInset, ToolCut,
    ToolLatheLoft, ToolBrush, ToolMapping, ToolPaint, ToolColour,
    // Mesh
    Transform, Replicate, MovePoints, SnapToGrid, MirrorX, MirrorY, MirrorZ, MirrorCloneX, MirrorCloneY,
    MirrorCloneZ, Flatten, Subdivide, DivideEdges, Poke, Connect,
    MergeCentre, MergeFirst, Collapse, Weld, Dissolve, TearOpen, Fill, Detach, FlipFaces, FlipDiagonal,
    Triangulate, JoinQuads, TwoSidedOn, TwoSidedOff, Bridge, Shell, MirrorMode, MirrorPlaneX, MirrorPlaneY,
    MirrorPlaneZ, MakeSymmetric, Problems,
    // Texture
    ApplyTexture,
    // View
    GridSmaller, GridLarger, GridSet, ToggleGrid, ToggleSnap, FlyMode, OrbitOrigin,
    OrbitSelection, FrameSelection, BackToOrigin, MaximiseView, ResetLayout, PaintWindow,
    ScaleFigure,
    // Help
    Controls, About,
    Count
};

struct Chord {
    int key = -1;       // GLFW_KEY_* (physical position); -1 = none
    bool shift = false;
    bool cmd = false;   // Cmd on macOS, Ctrl elsewhere
    bool valid() const { return key >= 0; }
    bool operator==(const Chord& o) const { return key == o.key && shift == o.shift && cmd == o.cmd; }
};

struct ActionInfo {
    Act act;
    const char* id;        // script name, e.g. "grid.larger"
    const char* label;     // menu text
    const char* keys;      // shortcut as shown, e.g. "Shift+S" (Cmd = Ctrl off macOS); "" = none
    int milestone;         // 0 = works now; otherwise the milestone that brings it
};

const ActionInfo& actionInfo(Act a);
// Finds an action by script id; false if unknown.
bool actionFromId(const std::string& id, Act& out);

// Parses "Cmd+Shift+T", "è", "+", "F1", "Esc", "Return", "1"...; false if unknown.
bool parseChord(const std::string& text, Chord& out);
Chord actionChord(Act a);
// Shortcut text for display on this platform ("Cmd" -> "Ctrl" off macOS).
std::string displayKeys(const char* keys);

} // namespace mazz
