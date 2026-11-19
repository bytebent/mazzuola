// Tools in the views: selecting (click, box), moving / scaling / rotating with handles in
// the 2D views, the primitives tool, placing the pivot, and the overlays drawn on top of
// the views (points, handles, the selection box, the pivot).
#include "app/editor.hpp"
#include "ui/theme.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace mazz {

namespace {
constexpr double kHandleRadius = 6;   // px, for grabbing a handle
constexpr double kPickPoint = 7;      // px
constexpr double kPickEdge = 5;       // px
constexpr double kDragStart = 3;      // px before a press counts as a drag

ImU32 rgba(Rgba c) { return IM_COL32(c.r, c.g, c.b, c.a); }
} // namespace

// ---------------------------------------------------------------- changes

void Editor::touch() { ++sceneVersion_; }

void Editor::windowFocused() {
    // Reference images (and later textures) may have been edited in another app.
    textures_.clear();
    touch();
}

bool Editor::edit(const std::string& what, const std::function<bool(Model&, Selection&)>& fn) {
    const bool changed = doc_.change(what, fn);
    if (changed) touch();
    return changed;
}

void Editor::setMode(SelectMode m) {
    if (m == mode_) return;
    doc_.sel = convertSelection(doc_.model.mesh, doc_.sel, mode_, m);
    mode_ = m;
    touch();
}

void Editor::setTool(Tool t) {
    if (t != Tool::Primitives && primActive_) cancelPrimitive();
    tool_ = t;
    if (t == Tool::Vertex) setMode(SelectMode::Vertex);
    touch();
}

std::set<int> Editor::movingVerts() const { return selectedVerts(doc_.model.mesh, doc_.sel, mode_); }

bool Editor::selectionBounds(Vec3& mn, Vec3& mx) const {
    const std::set<int> v = movingVerts();
    if (v.empty()) return false;
    bool first = true;
    for (int i : v) {
        if (i < 0 || i >= (int)doc_.model.mesh.verts.size()) continue;
        const Vec3& p = doc_.model.mesh.verts[i];
        if (first) { mn = mx = p; first = false; continue; }
        for (int a = 0; a < 3; ++a) { mn[a] = std::min(mn[a], p[a]); mx[a] = std::max(mx[a], p[a]); }
    }
    return !first;
}

Vec3 Editor::pivotPoint(PivotChoice c, const Vec3& typed) const {
    switch (c) {
    case PivotChoice::SelectionCentre: {
        Vec3 mn, mx;
        if (selectionBounds(mn, mx)) return (mn + mx) / 2;
        return doc_.model.pivot;
    }
    case PivotChoice::ModelPivot: return doc_.model.pivot;
    case PivotChoice::Origin: return {};
    case PivotChoice::Typed: return typed;
    }
    return {};
}

void Editor::selectAll() {
    const Mesh& m = doc_.model.mesh;
    Selection s;
    switch (mode_) {
    case SelectMode::Vertex:
        for (const Face& f : m.faces)
            if (!f.hidden)
                for (int k = 0; k < f.n; ++k) s.verts.insert(f.v[k]);
        break;
    case SelectMode::Edge:
        for (const Edge& e : m.edges(false)) s.edges.insert(e);
        break;
    default:
        for (int f = 0; f < (int)m.faces.size(); ++f)
            if (!m.faces[f].hidden) s.faces.insert(f);
        break;
    }
    doc_.sel = s;
    touch();
}

void Editor::selectNone() {
    if (doc_.sel.empty()) return;
    doc_.sel.clear();
    touch();
}

void Editor::deleteSelected() {
    const SelectMode mode = mode_;
    edit("Delete", [mode](Model& m, Selection& s) {
        if (s.empty()) return false;
        deleteSelection(m.mesh, s, mode);
        s.clear();
        return true;
    });
}

void Editor::duplicateSelected() {
    const SelectMode mode = mode_;
    edit("Duplicate", [mode](Model& m, Selection& s) {
        std::set<int> faces = (mode == SelectMode::Face || mode == SelectMode::Piece)
                                  ? s.faces
                                  : facesFullySelected(m.mesh, selectedVerts(m.mesh, s, mode));
        if (faces.empty()) return false;
        Selection copies;
        copies.faces = duplicateFaces(m.mesh, faces);
        s = convertSelection(m.mesh, copies, SelectMode::Face, mode);
        return true;
    });
}

void Editor::hideSelected(bool others) {
    const SelectMode mode = mode_;
    edit(others ? "Hide others" : "Hide", [mode, others](Model& m, Selection& s) {
        const std::set<int> touched = facesOfSelection(m.mesh, s, mode);
        if (touched.empty()) return false;
        if (others) {
            std::set<int> rest;
            for (int f = 0; f < (int)m.mesh.faces.size(); ++f)
                if (!touched.count(f) && !m.mesh.faces[f].hidden) rest.insert(f);
            if (rest.empty()) return false;
            setHidden(m.mesh, rest, true);
        } else {
            setHidden(m.mesh, touched, true);
            s.clear();
        }
        return true;
    });
}

void Editor::showAll() {
    edit("Show all", [](Model& m, Selection&) {
        bool any = false;
        for (Face& f : m.mesh.faces) {
            any = any || f.hidden;
            f.hidden = false;
        }
        return any;
    });
}

void Editor::snapSelectedToGrid() {
    const std::set<int> v = movingVerts();
    const double g = grid_.size;
    edit("Snap to grid", [&](Model& m, Selection&) {
        const Mesh before = m.mesh;
        snapPoints(m.mesh, v, g);
        for (size_t i = 0; i < m.mesh.verts.size(); ++i)
            if (m.mesh.verts[i].x != before.verts[i].x || m.mesh.verts[i].y != before.verts[i].y ||
                m.mesh.verts[i].z != before.verts[i].z)
                return true;
        return false;
    });
}

void Editor::growShrink(bool grow) {
    if (grow) growSelection(doc_.model.mesh, doc_.sel, mode_);
    else shrinkSelection(doc_.model.mesh, doc_.sel, mode_);
    touch();
}

void Editor::selectSimilar(Similar how) {
    doc_.sel.faces = similarFaces(doc_.model.mesh, doc_.sel.faces, how);
    touch();
}

void Editor::selectLoop(bool ring) {
    std::set<Edge> all;
    for (const Edge& e : doc_.sel.edges) {
        const std::set<Edge> more = ring ? edgeRing(doc_.model.mesh, e) : edgeLoop(doc_.model.mesh, e);
        all.insert(more.begin(), more.end());
    }
    doc_.sel.edges = all;
    touch();
}

// ---------------------------------------------------------------- primitives

void Editor::startPrimitive(PrimKind k) {
    setTool(Tool::Primitives);
    prim_.kind = k;
    if (!primActive_) {
        // A first preview at the origin, standing on the ground; drag in a 2D view to place
        // and size it, or type the values in Properties.
        prim_.centre = {0, 0, prim_.axis == 2 ? prim_.size.z / 2 : 0};
        if (k == PrimKind::Plane) prim_.centre.z = 0;
    }
    primActive_ = true;
    updatePrimPreview();
}

void Editor::updatePrimPreview() {
    primPreview_ = makePrimitive(prim_);
    touch();
}

void Editor::createPrimitive() {
    if (!primActive_) return;
    const Mesh made = makePrimitive(prim_);
    const SelectMode mode = mode_;
    const std::string name = std::string("Create ") + primKindName(prim_.kind);
    edit(name, [&](Model& m, Selection& s) {
        const int first = m.mesh.append(made);
        Selection created;
        for (int f = first; f < (int)m.mesh.faces.size(); ++f) created.faces.insert(f);
        s = convertSelection(m.mesh, created, SelectMode::Face, mode);
        return true;
    });
    primActive_ = false;
    touch();
}

void Editor::cancelPrimitive() {
    primActive_ = false;
    if (drag_ == DragKind::CreatePrim) drag_ = DragKind::None;
    touch();
}

void Editor::newSeed() {
    // Deterministic in tests (scripts), otherwise from the clock-free counter below.
    static uint32_t counter = 0x9E3779B9u;
    counter = counter * 1664525u + 1013904223u;
    prim_.seed = (counter >> 8) % 99999 + 1;
    if (primActive_) updatePrimPreview();
}

Vec3 Editor::figurePosition() const {
    Vec3 mn, mx;
    if (doc_.model.mesh.bounds(mn, mx)) return {mn.x - 50, (mn.y + mx.y) / 2, mn.z};
    return {-50, 0, 0};
}

// ---------------------------------------------------------------- picking

Projector Editor::projector(int view) const {
    Projector p;
    p.w = std::max(1.0f, viewImgSize_[view].x);
    p.h = std::max(1.0f, viewImgSize_[view].y);
    p.viewProj = views_[view].viewProj(p.w, p.h);
    p.perspective = views_[view].is3D();
    return p;
}

void Editor::clickSelect(int view, double mx, double my, bool add) {
    const Mesh& m = doc_.model.mesh;
    const Projector pr = projector(view);
    Selection& s = doc_.sel;
    if (!add) s.clear();
    auto toggle = [&](auto& set, const auto& item) {
        if (add && set.count(item)) set.erase(item);
        else set.insert(item);
    };
    switch (mode_) {
    case SelectMode::Vertex: {
        const int v = pickVertex(m, pr, mx, my, kPickPoint);
        if (v >= 0) toggle(s.verts, v);
        break;
    }
    case SelectMode::Edge: {
        Edge e;
        if (pickEdge(m, pr, mx, my, kPickEdge, e)) toggle(s.edges, e);
        break;
    }
    case SelectMode::Face: {
        const FaceHit h = pickFace(m, pr, mx, my);
        if (h.face >= 0) toggle(s.faces, h.face);
        break;
    }
    case SelectMode::Piece: {
        const FaceHit h = pickFace(m, pr, mx, my);
        if (h.face >= 0) {
            const std::set<int> piece = pieceOf(m, h.face);
            const bool had = s.faces.count(h.face) > 0;
            for (int f : piece) {
                if (add && had) s.faces.erase(f);
                else s.faces.insert(f);
            }
        }
        break;
    }
    }
    touch();
}

namespace {
// Selection bounds on screen (2D views): rectangle in view pixels.
struct ScreenBox {
    double x0, y0, x1, y1;
};
} // namespace

static bool selectionScreenBox(const Projector& pr, const Vec3& mn, const Vec3& mx, ScreenBox& b) {
    bool first = true;
    for (int i = 0; i < 8; ++i) {
        const Vec3 p{(i & 1) ? mx.x : mn.x, (i & 2) ? mx.y : mn.y, (i & 4) ? mx.z : mn.z};
        double x, y, d;
        if (!pr.project(p, x, y, d)) return false;
        if (first) { b = {x, y, x, y}; first = false; }
        b.x0 = std::min(b.x0, x);
        b.y0 = std::min(b.y0, y);
        b.x1 = std::max(b.x1, x);
        b.y1 = std::max(b.y1, y);
    }
    return true;
}

// Handle positions: 0..3 corners (top-left, top-right, bottom-right, bottom-left), 4..7
// side middles (top, right, bottom, left).
static void handlePositions(const ScreenBox& b, double out[8][2]) {
    const double cx = (b.x0 + b.x1) / 2, cy = (b.y0 + b.y1) / 2;
    const double p[8][2] = {{b.x0, b.y0}, {b.x1, b.y0}, {b.x1, b.y1}, {b.x0, b.y1},
                            {cx, b.y0},   {b.x1, cy},   {cx, b.y1},   {b.x0, cy}};
    for (int i = 0; i < 8; ++i) { out[i][0] = p[i][0]; out[i][1] = p[i][1]; }
}

bool Editor::handleAt(int view, double mx, double my, int& handle) const {
    if (views_[view].is3D() || tool_ == Tool::Primitives) return false;
    Vec3 mn, mxv;
    if (!selectionBounds(mn, mxv)) return false;
    ScreenBox b;
    if (!selectionScreenBox(projector(view), mn, mxv, b)) return false;
    // Handles sit a few pixels outside the box so they don't hide points.
    b.x0 -= 6; b.y0 -= 6; b.x1 += 6; b.y1 += 6;
    double pos[8][2];
    handlePositions(b, pos);
    const int count = handleMode_ == HandleMode::Scale ? 8 : 4;
    for (int i = 0; i < count; ++i)
        if (std::hypot(mx - pos[i][0], my - pos[i][1]) <= kHandleRadius) {
            handle = i;
            return true;
        }
    return false;
}

bool Editor::insideSelection(int view, double mx, double my) const {
    if (views_[view].is3D()) return false;
    Vec3 mn, mxv;
    if (!selectionBounds(mn, mxv)) return false;
    ScreenBox b;
    if (!selectionScreenBox(projector(view), mn, mxv, b)) return false;
    const double pad = 4; // thin selections (a single edge or point) are still grabbable
    return mx >= b.x0 - pad && mx <= b.x1 + pad && my >= b.y0 - pad && my <= b.y1 + pad;
}

// ---------------------------------------------------------------- dragging

void Editor::viewMouse(int view, double mx, double my, bool hovered, bool active) {
    ImGuiIO& io = ImGui::GetIO();
    const bool spaceHeld = ImGui::IsKeyDown(ImGuiKey_Space);
    // Press: decide what the drag will be.
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !spaceHeld && drag_ == DragKind::None) {
        dragView_ = view;
        dragX0_ = dragX_ = mx;
        dragY0_ = dragY_ = my;
        dragMoved_ = false;
        dragAdd_ = io.KeySuper || io.KeyCtrl;
        clickOnSelected_ = false;
        dragBase_ = {doc_.model, doc_.sel};
        selectionBounds(dragMin_, dragMax_);
        int h = -1;
        const bool is2D = !views_[view].is3D();
        if (pivotOpen_ && pivotPlacing_ && is2D) {
            drag_ = DragKind::PlacePivot;
        } else if (tool_ == Tool::Primitives) {
            drag_ = is2D ? DragKind::CreatePrim : DragKind::None;
        } else if (handleAt(view, mx, my, h)) {
            dragHandle_ = h;
            drag_ = handleMode_ == HandleMode::Scale ? DragKind::Scale : DragKind::Rotate;
        } else if (!dragAdd_ && insideSelection(view, mx, my)) {
            drag_ = DragKind::Move;
            clickOnSelected_ = true;
        } else {
            drag_ = DragKind::Box;
        }
    }
    if (drag_ == DragKind::None || dragView_ != view) return;
    if (active && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        dragX_ = mx;
        dragY_ = my;
        if (!dragMoved_ && std::hypot(mx - dragX0_, my - dragY0_) >= kDragStart) dragMoved_ = true;
        if (dragMoved_) applyDrag(view, mx, my, false);
        return;
    }
    // Released (or the button went up while we weren't looking).
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if (!dragMoved_) {
            // A click.
            if (drag_ == DragKind::Move && clickOnSelected_ && tool_ == Tool::Select) {
                // Hammer: clicking the selection switches its handles.
                // (Unless the click picked something that isn't selected: then select it.)
                const Selection before = doc_.sel;
                Selection probe = before;
                doc_.sel = before;
                clickSelect(view, mx, my, false);
                const bool sameThing = !doc_.sel.empty() && std::includes(before.verts.begin(), before.verts.end(), doc_.sel.verts.begin(), doc_.sel.verts.end()) &&
                                       std::includes(before.faces.begin(), before.faces.end(), doc_.sel.faces.begin(), doc_.sel.faces.end()) &&
                                       std::includes(before.edges.begin(), before.edges.end(), doc_.sel.edges.begin(), doc_.sel.edges.end());
                if (sameThing || doc_.sel.empty()) {
                    doc_.sel = before;
                    handleMode_ = handleMode_ == HandleMode::Scale ? HandleMode::Rotate : HandleMode::Scale;
                }
                (void)probe;
                touch();
            } else if (drag_ == DragKind::Box || drag_ == DragKind::Move) {
                clickSelect(view, mx, my, dragAdd_);
            } else if (drag_ == DragKind::PlacePivot) {
                applyDrag(view, mx, my, true);
            }
        } else {
            applyDrag(view, mx, my, true);
        }
        drag_ = DragKind::None;
        dragView_ = -1;
        touch();
    }
}

void Editor::cancelDrag() {
    if (drag_ == DragKind::Move || drag_ == DragKind::Scale || drag_ == DragKind::Rotate) {
        doc_.model = dragBase_.model;
        doc_.sel = dragBase_.sel;
    }
    drag_ = DragKind::None;
    dragView_ = -1;
    touch();
}

void Editor::applyDrag(int view, double mx, double my, bool final) {
    const View& v = views_[view];
    const OrthoCamera& cam = v.ortho;
    const OrthoAxes ax = orthoAxes(cam.dir);
    const double w = viewImgSize_[view].x, h = viewImgSize_[view].y;
    double u0, v0, u1, v1;
    cam.screenToPlane(dragX0_, dragY0_, w, h, u0, v0);
    cam.screenToPlane(mx, my, w, h, u1, v1);
    auto snap = [&](double x) { return grid_.snapValue(x); };

    switch (drag_) {
    case DragKind::Box: {
        if (!final) return; // the rectangle is drawn by the overlay
        Selection s = boxSelect(doc_.model.mesh, projector(view), dragX0_, dragY0_, mx, my, mode_);
        if (mode_ == SelectMode::Piece) s = convertSelection(doc_.model.mesh, s, SelectMode::Face, SelectMode::Piece);
        if (dragAdd_) {
            doc_.sel.verts.insert(s.verts.begin(), s.verts.end());
            doc_.sel.edges.insert(s.edges.begin(), s.edges.end());
            doc_.sel.faces.insert(s.faces.begin(), s.faces.end());
        } else {
            doc_.sel = s;
        }
        touch();
        return;
    }
    case DragKind::Move: {
        // The selection's lower corner (in this view's plane) snaps to the grid.
        double bu, bv;
        cam.worldToPlane(dragMin_, bu, bv);
        double bu2, bv2;
        cam.worldToPlane(dragMax_, bu2, bv2);
        const double cornerU = std::min(bu, bu2), cornerV = std::min(bv, bv2);
        const double du = snap(cornerU + (u1 - u0)) - cornerU;
        const double dv = snap(cornerV + (v1 - v0)) - cornerV;
        const Vec3 delta = ax.right() * du + ax.up() * dv;
        doc_.model = dragBase_.model;
        doc_.sel = dragBase_.sel;
        transformPoints(doc_.model.mesh, selectedVerts(doc_.model.mesh, doc_.sel, mode_), translation(delta));
        if (final) {
            if (length(delta) > 1e-12) doc_.commitFrom(dragBase_, "Move");
            else doc_.model = dragBase_.model;
        }
        touch();
        return;
    }
    case DragKind::Scale: {
        double au, av, bu, bv;
        cam.worldToPlane(dragMin_, au, av);
        cam.worldToPlane(dragMax_, bu, bv);
        double umin = std::min(au, bu), umax = std::max(au, bu), vmin = std::min(av, bv), vmax = std::max(av, bv);
        const double ou0 = umin, ou1 = umax, ov0 = vmin, ov1 = vmax;
        // Which sides this handle moves (screen y is down: "top" is vmax).
        const int hnd = dragHandle_;
        const bool left = hnd == 0 || hnd == 3 || hnd == 7, right = hnd == 1 || hnd == 2 || hnd == 5;
        const bool top = hnd == 0 || hnd == 1 || hnd == 4, bottom = hnd == 2 || hnd == 3 || hnd == 6;
        const double du = u1 - u0, dv = v1 - v0;
        const double minSize = 0.01;
        if (left) umin = std::min(snap(umin + du), umax - minSize);
        if (right) umax = std::max(snap(umax + du), umin + minSize);
        if (top) vmax = std::max(snap(vmax + dv), vmin + minSize);
        if (bottom) vmin = std::min(snap(vmin + dv), vmax - minSize);
        Vec3 f{1, 1, 1};
        Vec3 anchor = (dragMin_ + dragMax_) / 2;
        if ((left || right) && ou1 - ou0 > 1e-9) {
            f[ax.u] = (umax - umin) / (ou1 - ou0);
            const double fixedU = left ? ou1 : ou0;
            anchor[ax.u] = fixedU * ax.su;
            // scaled about the fixed side, then shifted so the moved side lands where wanted
        }
        if ((top || bottom) && ov1 - ov0 > 1e-9) {
            f[ax.v] = (vmax - vmin) / (ov1 - ov0);
            const double fixedV = top ? ov0 : ov1;
            anchor[ax.v] = fixedV * ax.sv;
        }
        doc_.model = dragBase_.model;
        doc_.sel = dragBase_.sel;
        transformPoints(doc_.model.mesh, selectedVerts(doc_.model.mesh, doc_.sel, mode_), about(scaling(f), anchor));
        if (final) {
            if (f.x != 1 || f.y != 1 || f.z != 1) doc_.commitFrom(dragBase_, "Scale");
            else doc_.model = dragBase_.model;
        }
        touch();
        return;
    }
    case DragKind::Rotate: {
        const Vec3 c = (dragMin_ + dragMax_) / 2;
        double cu, cv;
        cam.worldToPlane(c, cu, cv);
        const double a0 = std::atan2(v0 - cv, u0 - cu), a1 = std::atan2(v1 - cv, u1 - cu);
        double deg = degrees(a1 - a0);
        if (grid_.snap) deg = std::round(deg / 15.0) * 15.0;
        // Counter-clockwise in the view = around the axis pointing at the viewer.
        const Vec3 toViewer = cross(ax.right(), ax.up());
        int k = 0;
        for (int a = 0; a < 3; ++a)
            if (std::fabs(toViewer[a]) > 0.5) k = a;
        Vec3 r;
        r[k] = deg * (toViewer[k] > 0 ? 1 : -1);
        doc_.model = dragBase_.model;
        doc_.sel = dragBase_.sel;
        transformPoints(doc_.model.mesh, selectedVerts(doc_.model.mesh, doc_.sel, mode_), about(rotationXYZ(r), c));
        if (final) {
            if (std::fabs(deg) > 1e-12) doc_.commitFrom(dragBase_, "Rotate");
            else doc_.model = dragBase_.model;
        }
        touch();
        return;
    }
    case DragKind::CreatePrim: {
        const double su0 = snap(u0), sv0 = snap(v0), su1 = snap(u1), sv1 = snap(v1);
        const double wU = std::max(std::fabs(su1 - su0), grid_.snap ? grid_.size : 0.5);
        const double wV = std::max(std::fabs(sv1 - sv0), grid_.snap ? grid_.size : 0.5);
        const double mu = (su0 + su1) / 2, mv = (sv0 + sv1) / 2;
        const Vec3 toViewer = cross(ax.right(), ax.up());
        int depthAxis = 0;
        for (int a = 0; a < 3; ++a)
            if (std::fabs(toViewer[a]) > 0.5) depthAxis = a;
        Vec3 centre = prim_.centre;
        if (!primActive_) {
            // A new shape stands on the ground when drawn from above or below; from the
            // side views it is centred on the origin's plane.
            centre[depthAxis] = (depthAxis == 2 && prim_.kind != PrimKind::Plane) ? prim_.size.z / 2 : 0;
        }
        const Vec3 inPlane = ax.right() * mu + ax.up() * mv;
        centre[ax.u] = inPlane[ax.u];
        centre[ax.v] = inPlane[ax.v];
        prim_.centre = centre;
        prim_.size[ax.u] = wU;
        prim_.size[ax.v] = wV;
        prim_.axis = depthAxis;
        if (prim_.kind == PrimKind::Plane) prim_.centre[depthAxis] = 0;
        primActive_ = true;
        updatePrimPreview();
        return;
    }
    case DragKind::PlacePivot: {
        if (!final) return;
        Vec3 p = doc_.model.pivot;
        const Vec3 at = cam.planeToWorld(snap(u1), snap(v1));
        p[ax.u] = at[ax.u];
        p[ax.v] = at[ax.v];
        pivotTyped_ = p;
        edit("Move pivot", [p](Model& m, Selection&) {
            if (m.pivot.x == p.x && m.pivot.y == p.y && m.pivot.z == p.z) return false;
            m.pivot = p;
            return true;
        });
        pivotPlacing_ = false;
        return;
    }
    case DragKind::None: return;
    }
}

// ---------------------------------------------------------------- overlays

void Editor::drawOverlay(int view, ImDrawList* dl, ImVec2 o, ImVec2 size) {
    const Projector pr = projector(view);
    const Mesh& m = doc_.model.mesh;
    dl->PushClipRect(o, {o.x + size.x, o.y + size.y}, true);
    auto at = [&](const Vec3& p, ImVec2& out) {
        double x, y, d;
        if (!pr.project(p, x, y, d)) return false;
        out = {o.x + (float)x, o.y + (float)y};
        return true;
    };
    const ImU32 pink = rgba(viewcol::kSelection);
    // Points (vertex mode): all visible ones small, selected ones bigger and pink.
    if (mode_ == SelectMode::Vertex) {
        std::vector<bool> vis(m.verts.size(), false);
        for (const Face& f : m.faces)
            if (!f.hidden)
                for (int k = 0; k < f.n; ++k) vis[f.v[k]] = true;
        for (int i = 0; i < (int)m.verts.size(); ++i) {
            if (!vis[i]) continue;
            ImVec2 p;
            if (!at(m.verts[i], p)) continue;
            const bool sel = doc_.sel.verts.count(i) > 0;
            const float r = sel ? 3.5f : 2.5f;
            dl->AddRectFilled({p.x - r, p.y - r}, {p.x + r, p.y + r}, sel ? pink : IM_COL32(50, 52, 60, 230));
        }
    }
    // The model's pivot: a small cross with a ring.
    {
        ImVec2 p;
        if (at(doc_.model.pivot, p)) {
            const ImU32 c = IM_COL32(110, 70, 160, 220);
            dl->AddLine({p.x - 7, p.y}, {p.x + 7, p.y}, c, 1.5f);
            dl->AddLine({p.x, p.y - 7}, {p.x, p.y + 7}, c, 1.5f);
            dl->AddCircle(p, 4.5f, c, 12, 1.5f);
        }
    }
    // Handles around the selection (2D views, select and vertex tools).
    Vec3 mn, mx;
    if (!views_[view].is3D() && tool_ != Tool::Primitives && selectionBounds(mn, mx) && drag_ != DragKind::Box) {
        ScreenBox b;
        if (selectionScreenBox(pr, mn, mx, b)) {
            ScreenBox hb{b.x0 - 6, b.y0 - 6, b.x1 + 6, b.y1 + 6};
            dl->AddRect({o.x + (float)hb.x0, o.y + (float)hb.y0}, {o.x + (float)hb.x1, o.y + (float)hb.y1},
                        IM_COL32(236, 72, 170, 110), 0.0f, 0, 1.0f);
            double pos[8][2];
            handlePositions(hb, pos);
            const int count = handleMode_ == HandleMode::Scale ? 8 : 4;
            for (int i = 0; i < count; ++i) {
                const ImVec2 p{o.x + (float)pos[i][0], o.y + (float)pos[i][1]};
                if (handleMode_ == HandleMode::Scale) {
                    dl->AddRectFilled({p.x - 3.5f, p.y - 3.5f}, {p.x + 3.5f, p.y + 3.5f}, IM_COL32(255, 255, 255, 255));
                    dl->AddRect({p.x - 3.5f, p.y - 3.5f}, {p.x + 3.5f, p.y + 3.5f}, pink, 0.0f, 0, 1.5f);
                } else {
                    dl->AddCircleFilled(p, 4.5f, IM_COL32(255, 255, 255, 255), 12);
                    dl->AddCircle(p, 4.5f, pink, 12, 1.5f);
                }
            }
        }
    }
    // Box selection rectangle.
    if (drag_ == DragKind::Box && dragView_ == view && dragMoved_) {
        const ImVec2 a{o.x + (float)std::min(dragX0_, dragX_), o.y + (float)std::min(dragY0_, dragY_)};
        const ImVec2 b{o.x + (float)std::max(dragX0_, dragX_), o.y + (float)std::max(dragY0_, dragY_)};
        dl->AddRectFilled(a, b, IM_COL32(236, 72, 170, 30));
        dl->AddRect(a, b, pink, 0.0f, 0, 1.0f);
    }
    // Size of the primitive being drawn.
    if (drag_ == DragKind::CreatePrim && dragView_ == view && dragMoved_) {
        const OrthoAxes ax = orthoAxes(views_[view].dir);
        char t[64];
        std::snprintf(t, sizeof t, "%g x %g cm", prim_.size[ax.u], prim_.size[ax.v]);
        dl->AddText({o.x + (float)dragX_ + 12, o.y + (float)dragY_ + 8}, IM_COL32(20, 90, 130, 255), t);
    }
    dl->PopClipRect();
}

} // namespace mazz
