#include "tools/select_tool.hpp"
#include "core/pick.hpp"
#include "core/transform.hpp"
#include "render/view_draw.hpp" // selection colour
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>

namespace mazz {

namespace {
constexpr double kDragStart = 3; // px before a press becomes a drag
constexpr double kRotateStep = 15;

ImU32 selColour() {
    const Rgba c = viewcol::kSelection;
    return IM_COL32(c.r, c.g, c.b, 255);
}

Selection pieceSelection(const std::set<int>& faces) {
    Selection s;
    s.mode = SelectMode::Piece;
    s.faces = faces;
    return s;
}
} // namespace

const char* SelectTool::hint() const {
    return "click: select  Cmd+click: add/remove  drag: move  Shift+drag: copy  "
           "Option+drag corner: rotate  Delete: delete  Esc: select nothing";
}

void SelectTool::press(ToolEnv& env, const Pointer& p) {
    action_ = Action::Pending;
    view_ = p.view;
    sx0_ = sx1_ = p.sx;
    sy0_ = sy1_ = p.sy;
    if (!p.g.is3D()) p.g.toPlane(p.sx, p.sy, u0_, v0_);
    shiftAtPress_ = p.shift;
    cmdAtPress_ = p.cmd;
    insideAtPress_ = false;
    grab_ = BoxGrab{};
    const Document& doc = env.doc;
    if (doc.sel.empty() || !selectionBounds(doc.mesh, doc.sel, mn0_, mx0_)) return;
    if (p.g.is3D()) {
        grab_ = hitRings(p.g, mn0_, mx0_, p.sx, p.sy);
        if (grab_.kind == BoxGrab::Ring) action_ = Action::Ring;
        return;
    }
    grab_ = hitBox2D(p.g, mn0_, mx0_, p.sx, p.sy, p.alt);
    if (grab_.kind == BoxGrab::Handle) action_ = Action::Resize;
    else if (grab_.kind == BoxGrab::Rotate) action_ = Action::Rotate;
    else if (grab_.kind == BoxGrab::Inside) insideAtPress_ = true;
}

void SelectTool::beginEdit(ToolEnv& env, bool copy) {
    Document& doc = env.doc;
    doc.beginLive();
    if (copy) {
        const std::set<int> made = copyFaces(doc.live(), doc.sel.faces);
        doc.select(pieceSelection(made));
    }
    start_.clear();
    for (int v : selectedVerts(doc.mesh, doc.sel)) start_[v] = doc.mesh.verts[v];
}

void SelectTool::drag(ToolEnv& env, const Pointer& p) {
    if (action_ == Action::None || p.view != view_) return;
    sx1_ = p.sx;
    sy1_ = p.sy;
    Document& doc = env.doc;
    if (action_ == Action::Pending) {
        if (std::hypot(p.sx - sx0_, p.sy - sy0_) < kDragStart) return;
        if (insideAtPress_ && !cmdAtPress_) {
            action_ = Action::Move;
            beginEdit(env, shiftAtPress_);
        } else {
            action_ = Action::Rect;
            env.touch();
            return;
        }
    } else if ((action_ == Action::Resize || action_ == Action::Rotate || action_ == Action::Ring) &&
               !doc.liveActive()) {
        if (std::hypot(p.sx - sx0_, p.sy - sy0_) < kDragStart) return;
        beginEdit(env, false);
    }
    if (action_ == Action::Rect) {
        env.touch();
        return;
    }
    const Vec3 centre = (mn0_ + mx0_) / 2;
    double u = 0, v = 0;
    if (!p.g.is3D()) p.g.toPlane(p.sx, p.sy, u, v);
    MeshEdit& e = doc.live();
    if (action_ == Action::Move) {
        const Vec3 d = moveOffset(p.g, env.grid, mn0_, u - u0_, v - v0_);
        for (const auto& [i, s] : start_) e.setVert(i, s + d);
    } else if (action_ == Action::Resize) {
        Vec3 mn, mx;
        resizeBox(p.g, env.grid, mn0_, mx0_, grab_.su, grab_.sv, u - u0_, v - v0_, mn, mx);
        for (const auto& [i, s] : start_) e.setVert(i, stretchInBox(s, mn0_, mx0_, mn, mx));
    } else {
        const Vec3 axis = action_ == Action::Ring ? axisVec(grab_.ringAxis) : viewAxisTowardViewer(p.g);
        const double raw = dragAngle(p.g, centre, sx0_, sy0_, p.sx, p.sy, axis);
        const double angle = snapAngle(raw, p.shift ? 0 : kRotateStep);
        for (const auto& [i, s] : start_) e.setVert(i, rotateAbout(s, centre, axis, angle));
    }
    env.touch();
}

void SelectTool::release(ToolEnv& env, const Pointer& p) {
    Document& doc = env.doc;
    switch (action_) {
    case Action::Pending:
        clickSelect(env, p);
        break;
    case Action::Rect: {
        const std::set<int> pieces = piecesInRect(doc.mesh, p.g, sx0_, sy0_, sx1_, sy1_);
        std::set<int> faces = facesOfPieces(doc.mesh, pieces);
        if (cmdAtPress_ && doc.sel.mode == SelectMode::Piece) faces.insert(doc.sel.faces.begin(), doc.sel.faces.end());
        doc.select(pieceSelection(faces));
        break;
    }
    case Action::Move:
        doc.commitLive(shiftAtPress_ ? "Copy" : "Move");
        break;
    case Action::Resize:
        if (doc.liveActive()) doc.commitLive("Resize");
        break;
    case Action::Rotate:
    case Action::Ring:
        if (doc.liveActive()) doc.commitLive("Rotate");
        break;
    case Action::None:
        break;
    }
    action_ = Action::None;
    view_ = -1;
    start_.clear();
    env.touch();
}

void SelectTool::clickSelect(ToolEnv& env, const Pointer& p) {
    Document& doc = env.doc;
    const std::vector<int> under = piecesAt(doc.mesh, p.g, p.sx, p.sy);
    const bool sameSpot = p.view == lastClickView_ && std::hypot(p.sx - lastClickX_, p.sy - lastClickY_) <= 3;
    lastClickX_ = p.sx;
    lastClickY_ = p.sy;
    lastClickView_ = p.view;
    if (under.empty()) {
        if (!cmdAtPress_) doc.select(pieceSelection({}));
        return;
    }
    if (cmdAtPress_) {
        // Add or remove the piece in front.
        const std::set<int> faces = facesOfPieces(doc.mesh, {under[0]});
        Selection s = doc.sel.mode == SelectMode::Piece ? doc.sel : pieceSelection({});
        const bool has = s.faces.count(*faces.begin()) > 0;
        for (int f : faces) {
            if (has) s.faces.erase(f);
            else s.faces.insert(f);
        }
        doc.select(s);
        return;
    }
    // Clicking again on the same spot: the next piece underneath.
    size_t pick = 0;
    if (sameSpot && under.size() > 1) {
        for (size_t k = 0; k < under.size(); ++k)
            if (doc.sel.faces == facesOfPieces(doc.mesh, {under[k]})) {
                pick = (k + 1) % under.size();
                break;
            }
    }
    doc.select(pieceSelection(facesOfPieces(doc.mesh, {under[pick]})));
}

void SelectTool::hover(ToolEnv& env, const Pointer& p) {
    int hot = -1;
    Vec3 mn, mx;
    if (p.g.is3D() && !env.doc.sel.empty() && selectionBounds(env.doc.mesh, env.doc.sel, mn, mx)) {
        const BoxGrab g = hitRings(p.g, mn, mx, p.sx, p.sy);
        if (g.kind == BoxGrab::Ring) hot = g.ringAxis;
    }
    if (hot != hotRing_) {
        hotRing_ = hot;
        env.touch();
    }
}

bool SelectTool::key(ToolEnv& env, int key, bool, bool) {
    // Esc in the middle of a drag puts everything back; otherwise it's "select nothing".
    if (key == GLFW_KEY_ESCAPE && action_ != Action::None) {
        cancel(env);
        return true;
    }
    return false;
}

void SelectTool::cancel(ToolEnv& env) {
    if (env.doc.liveActive()) env.doc.cancelLive();
    action_ = Action::None;
    view_ = -1;
    start_.clear();
    env.touch();
}

void SelectTool::overlay(const ToolEnv& env, const ViewGeom& g, int view, ImDrawList* dl) const {
    const Document& doc = env.doc;
    Vec3 mn, mx;
    if (!doc.sel.empty() && selectionBounds(doc.mesh, doc.sel, mn, mx)) {
        if (g.is3D()) drawRings(dl, g, mn, mx, action_ == Action::Ring ? grab_.ringAxis : hotRing_);
        else drawBox2D(dl, g, mn, mx, selColour(), true, false);
    }
    if (action_ == Action::Rect && view == view_) {
        const ImVec2 a((float)std::min(sx0_, sx1_), (float)std::min(sy0_, sy1_));
        const ImVec2 b((float)std::max(sx0_, sx1_), (float)std::max(sy0_, sy1_));
        dl->AddRectFilled(a, b, IM_COL32(236, 92, 178, 30));
        dl->AddRect(a, b, selColour(), 0, 0, 1.0f);
    }
}

} // namespace mazz
