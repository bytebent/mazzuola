#include "ui/actions.hpp"
#include <GLFW/glfw3.h>
#include <cctype>
#include <cstring>
#include <vector>

namespace mazz {

namespace {
// Order must match enum Act.
const ActionInfo kActions[] = {
    {Act::New, "file.new", "New", "Cmd+N", 0},
    {Act::Open, "file.open", "Open...", "Cmd+O", 0},
    {Act::Save, "file.save", "Save", "Cmd+S", 0},
    {Act::SaveAs, "file.saveas", "Save as...", "Cmd+Shift+S", 0},
    {Act::ExportGlb, "file.exportglb", "glTF binary (.glb)...", "", 0},
    {Act::ExportGltf, "file.exportgltf", "glTF (.gltf + .bin)...", "", 0},
    {Act::ExportObj, "file.exportobj", "Wavefront (.obj + .mtl)...", "", 0},
    {Act::ExportAgain, "file.exportagain", "Export again", "Cmd+Shift+E", 0},
    {Act::Quit, "file.quit", "Quit", "Cmd+Q", 0},

    {Act::Undo, "edit.undo", "Undo", "Cmd+Z", 0},
    {Act::Redo, "edit.redo", "Redo", "Cmd+Shift+Z", 0},
    {Act::Duplicate, "edit.duplicate", "Duplicate", "Cmd+D", 0},
    {Act::Delete, "edit.delete", "Delete", "Delete", 0},
    {Act::SelectAll, "edit.selectall", "Select all", "Cmd+A", 0},
    {Act::SelectNone, "edit.selectnone", "Select none", "Esc", 0},

    {Act::ModeVertex, "select.vertices", "Vertices", "1", 0},
    {Act::ModeEdge, "select.edges", "Edges", "2", 0},
    {Act::ModeFace, "select.faces", "Faces", "3", 0},
    {Act::ModePiece, "select.pieces", "Connected pieces", "4", 0},
    {Act::SelectGrow, "select.grow", "Grow selection", "Cmd++", 0},
    {Act::SelectShrink, "select.shrink", "Shrink selection", "Cmd+\xC3\xA8", 0},
    {Act::SelectLoop, "select.loop", "Edge loop (double-click an edge)", "", 0},
    {Act::SelectRing, "select.ring", "Edge ring (Shift+double-click an edge)", "", 0},
    {Act::Hide, "select.hide", "Hide selected", "H", 1},
    {Act::HideOthers, "select.hideothers", "Hide others", "Shift+H", 1},
    {Act::ShowAll, "select.showall", "Show all", "Cmd+U", 1},

    {Act::ToolSelect, "tool.select", "Select", "Shift+S", 0},
    {Act::ToolVertex, "tool.vertex", "Vertex", "Shift+V", 0},
    {Act::ToolPrimitives, "tool.primitives", "Primitives", "Shift+B", 0},
    {Act::ToolDraw, "tool.draw", "Draw", "Shift+D", 0},
    {Act::ToolPushPull, "tool.pushpull", "Push/pull", "Shift+F", 0},
    {Act::ToolExtrude, "tool.extrude", "Extrude", "Shift+E", 0},
    {Act::ToolInset, "tool.inset", "Inset", "Shift+I", 0},
    {Act::ToolCut, "tool.cut", "Cut (knife, loop cut)", "Shift+X", 0},
    {Act::ToolLatheLoft, "tool.latheloft", "Lathe / loft", "Shift+C", 3},
    {Act::ToolBrush, "tool.brush", "Vertex brush", "Shift+G", 4},
    {Act::ToolMapping, "tool.mapping", "Mapping", "Shift+A", 5},
    {Act::ToolPaint, "tool.paint", "Paint", "Shift+P", 6},
    {Act::ToolColour, "tool.colour", "Vertex colour", "Shift+O", 7},

    {Act::Transform, "mesh.transform", "Move / rotate / scale...", "Cmd+T", 0},
    {Act::Replicate, "mesh.replicate", "Multi-Replicate...", "Cmd+Shift+D", 0},
    {Act::MovePoints, "mesh.movepoints", "Move points exactly...", "Cmd+Shift+T", 0},
    {Act::SnapToGrid, "mesh.snap", "Snap points to grid", "Cmd+B", 0},
    {Act::MirrorX, "mesh.mirrorx", "Mirror X", "", 0},
    {Act::MirrorY, "mesh.mirrory", "Mirror Y", "", 0},
    {Act::MirrorZ, "mesh.mirrorz", "Mirror Z", "", 0},
    {Act::MirrorCloneX, "mesh.mirrorclonex", "Mirror clone X", "", 0},
    {Act::MirrorCloneY, "mesh.mirrorcloney", "Mirror clone Y", "", 0},
    {Act::MirrorCloneZ, "mesh.mirrorclonez", "Mirror clone Z", "", 0},
    {Act::Flatten, "mesh.flatten", "Flatten...", "", 0},
    {Act::Subdivide, "mesh.subdivide", "Subdivide...", "", 0},
    {Act::DivideEdges, "mesh.divideedges", "Divide edges...", "", 0},
    {Act::Poke, "mesh.poke", "Poke...", "", 0},
    {Act::Connect, "mesh.connect", "Connect two points", "", 0},
    {Act::MergeCentre, "mesh.mergecentre", "Merge at centre", "", 0},
    {Act::MergeFirst, "mesh.mergefirst", "Merge at first selected point", "", 0},
    {Act::Collapse, "mesh.collapse", "Collapse edges", "", 0},
    {Act::Weld, "mesh.weld", "Weld by distance...", "", 0},
    {Act::Dissolve, "mesh.dissolve", "Dissolve", "", 0},
    {Act::TearOpen, "mesh.tearopen", "Tear open", "", 0},
    {Act::Fill, "mesh.fill", "Fill", "", 0},
    {Act::Detach, "mesh.detach", "Detach", "", 0},
    {Act::FlipFaces, "mesh.flip", "Flip faces", "", 0},
    {Act::FlipDiagonal, "mesh.flipdiagonal", "Flip diagonal", "", 0},
    {Act::Triangulate, "mesh.triangulate", "Triangulate", "", 0},
    {Act::JoinQuads, "mesh.joinquads", "Join into quads", "", 0},
    {Act::TwoSidedOn, "mesh.twosidedon", "Double-sided on", "", 0},
    {Act::TwoSidedOff, "mesh.twosidedoff", "Double-sided off", "", 0},
    {Act::Bridge, "mesh.bridge", "Bridge...", "", 0},
    {Act::Shell, "mesh.shell", "Shell...", "", 0},
    {Act::Problems, "mesh.problems", "Problems list", "Cmd+Shift+O", 3},

    {Act::ApplyTexture, "texture.apply", "Apply current texture", "Shift+T", 5},

    {Act::GridSmaller, "grid.smaller", "Smaller grid", "\xC3\xA8", 0},
    {Act::GridLarger, "grid.larger", "Larger grid", "+", 0},
    {Act::GridSet, "grid.set", "Set grid size...", "", 0},
    {Act::ToggleGrid, "grid.toggle", "Show grid", "Shift+R", 0},
    {Act::ToggleSnap, "grid.snap", "Snap to grid", "Shift+W", 0},
    {Act::FlyMode, "view.fly", "Fly mode (3D view)", "Z", 0},
    {Act::OrbitOrigin, "view.orbitorigin", "Orbit around origin", "", 0},
    {Act::OrbitSelection, "view.orbitselection", "Orbit around selection", "", 0},
    {Act::FrameSelection, "view.frame", "Frame selection", "Cmd+E", 1},
    {Act::BackToOrigin, "view.origin", "Back to origin", "Cmd+0", 0},
    {Act::MaximiseView, "view.maximise", "Maximise view under the cursor", "", 0},
    {Act::ResetLayout, "view.resetlayout", "Reset layout", "", 0},
    {Act::PaintWindow, "view.paintwindow", "Paint window", "", 0},
    {Act::ScaleFigure, "view.scalefigure", "Scale figure", "", 0},

    {Act::Controls, "help.controls", "Controls", "F1", 0},
    {Act::About, "help.about", "About Mazzuola", "", 0},
};
static_assert(sizeof kActions / sizeof kActions[0] == (size_t)Act::Count, "action table out of sync");

struct KeyName {
    const char* name;
    int key;
};
const KeyName kKeyNames[] = {
    {"\xC3\xA8", GLFW_KEY_LEFT_BRACKET},  // è: Italian key right of P
    {"+", GLFW_KEY_RIGHT_BRACKET},        // +: the next one
    {"Esc", GLFW_KEY_ESCAPE},
    {"Return", GLFW_KEY_ENTER},
    {"Delete", GLFW_KEY_BACKSPACE},       // the Mac "delete" key
    {"Space", GLFW_KEY_SPACE},
    {"Tab", GLFW_KEY_TAB},
    {"F1", GLFW_KEY_F1},
};

bool ieq(const std::string& a, const char* b) {
    if (a.size() != std::strlen(b)) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
    return true;
}
} // namespace

const ActionInfo& actionInfo(Act a) { return kActions[(int)a]; }

bool actionFromId(const std::string& id, Act& out) {
    for (const ActionInfo& a : kActions)
        if (id == a.id) { out = a.act; return true; }
    return false;
}

bool parseChord(const std::string& text, Chord& out) {
    out = Chord{};
    if (text.empty()) return false;
    // Split on '+', but a trailing "+" (or the whole text "+") is the key itself.
    std::vector<std::string> parts;
    std::string cur;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '+' && !cur.empty()) { parts.push_back(cur); cur.clear(); }
        else cur += text[i];
    }
    if (!cur.empty()) parts.push_back(cur);
    if (parts.empty()) return false;
    for (size_t i = 0; i + 1 < parts.size(); ++i) {
        if (ieq(parts[i], "Shift")) out.shift = true;
        else if (ieq(parts[i], "Cmd") || ieq(parts[i], "Ctrl")) out.cmd = true;
        else return false;
    }
    const std::string& k = parts.back();
    for (const KeyName& kn : kKeyNames)
        if (k == kn.name || ieq(k, kn.name)) { out.key = kn.key; return true; }
    if (k.size() == 1) {
        const char ch = (char)std::toupper((unsigned char)k[0]);
        if (ch >= 'A' && ch <= 'Z') { out.key = GLFW_KEY_A + (ch - 'A'); return true; }
        if (ch >= '0' && ch <= '9') { out.key = GLFW_KEY_0 + (ch - '0'); return true; }
    }
    return false;
}

Chord actionChord(Act a) {
    Chord c;
    const char* k = actionInfo(a).keys;
    if (k && *k) parseChord(k, c);
    return c;
}

std::string displayKeys(const char* keys) {
    std::string s = keys ? keys : "";
#if !defined(__APPLE__)
    for (size_t p = s.find("Cmd"); p != std::string::npos; p = s.find("Cmd", p)) s.replace(p, 3, "Ctrl");
#endif
    return s;
}

} // namespace mazz
