#pragma once
// Tool interface (ARCHITECTURE §4), as in Scalpellino. Each tool turns mouse and key
// input into commands on the document and draws its own handles and previews on top of
// the views. The editor only routes input to the active tool; it has no per-tool code.
//
// Rules:
//  - Tools change the document only through Document::apply / live edits (undoable) or
//    selection changes.
//  - While busy (a drag, a preview), a tool owns the mouse and keys. cancel() is called
//    before a tool switch, undo, redo or a file load.
#include "core/document.hpp"
#include "core/grid.hpp"
#include "core/view_geom.hpp"
#include "imgui.h"
#include <functional>

namespace mazz {

// The mouse in one view.
struct Pointer {
    int view = -1;          // 0-3
    ViewGeom g;             // that view's geometry
    double sx = 0, sy = 0;  // screen position (logical px)
    bool cmd = false, shift = false, alt = false; // alt = Option on macOS
    int clicks = 1;         // at a press: 2 = double-click
};

// Bounds of the last shape made or selected: new shapes take their depth from it.
struct LastBox {
    Vec3 mn{0, 0, 0}, mx{64, 64, 64};
    bool valid = false;
};

struct ToolEnv {
    Document& doc;
    const Grid& grid;
    LastBox& lastBox;
    std::function<void()> touch; // the model or overlay changed: redraw the views
};

// Grey, wrapped text for tool notes in Properties.
inline void dimWrapped(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

class Tool {
public:
    virtual ~Tool() = default;
    virtual const char* name() const = 0;
    // What the gestures do, for the status bar.
    virtual const char* hint() const = 0;

    virtual void press(ToolEnv&, const Pointer&) {}
    virtual void drag(ToolEnv&, const Pointer&) {}
    virtual void release(ToolEnv&, const Pointer&) {}
    // Mouse moved over a view with no button down.
    virtual void hover(ToolEnv&, const Pointer&) {}
    // Return, Esc, Delete... (GLFW key codes). True if the tool used the key.
    virtual bool key(ToolEnv&, int /*key*/, bool /*shift*/, bool /*cmd*/) { return false; }

    // Handles, previews and highlights for one view, drawn over its image.
    virtual void overlay(const ToolEnv&, const ViewGeom&, int /*view*/, ImDrawList*) const {}
    // The tool's section in the Properties panel.
    virtual void properties(ToolEnv&) {}

    // Every frame, before the views are drawn (tools that follow the selection).
    virtual void update(ToolEnv&) {}
    // The tool was just chosen.
    virtual void activate(ToolEnv&) {}

    // Drop whatever is in progress (a drag rolls back, a preview disappears).
    virtual void cancel(ToolEnv&) {}
    // In the middle of something (input belongs to the tool).
    virtual bool busy() const { return false; }
};

} // namespace mazz
