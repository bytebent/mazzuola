#include "tools/cut_tool.hpp"
#include "core/numbers.hpp"
#include "core/select_ops.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>

namespace mazz {

namespace {
constexpr double kSnapPx = 8;
const ImU32 kSnapCol[4] = {0, IM_COL32(40, 170, 60, 255), IM_COL32(0, 170, 200, 255), IM_COL32(220, 50, 50, 255)};
const ImU32 kLineCol = IM_COL32(236, 92, 178, 255);
const ImU32 kBadCol = IM_COL32(230, 60, 60, 255);
const ImU32 kRingCol = IM_COL32(0, 170, 200, 255);

bool screen(const ViewGeom& g, const Vec3& p, ImVec2& out) {
    double x, y;
    if (!g.project(p, x, y)) return false;
    out = ImVec2((float)x, (float)y);
    return true;
}
} // namespace

const char* CutTool::hint() const {
    if (mode_ == Mode::Loop)
        return picked_.valid() ? "type the cuts and position  Return: cut  Esc: drop it  click another edge: pick that ring"
                               : "point at an edge: its ring  click: pick it";
    return knife_.empty() ? "click a point on an edge to start cutting"
                          : "click on edges (two in a row on one face)  Return: cut  Backspace: undo a point  Esc: cancel";
}

CutTool::Cursor CutTool::snapAt(const Mesh& m, const ViewGeom& g, double sx, double sy) const {
    Cursor c;
    // A point, else an edge (its midpoint, or the spot on it nearest the cursor).
    const std::set<int> pts = pointsAt(m, g, sx, sy, kSnapPx);
    if (!pts.empty()) {
        Vec3 ro, rd;
        g.ray(sx, sy, ro, rd);
        int best = -1;
        double bestNear = 0;
        for (int v : pts) {
            const double n = -dot(m.verts[v], rd);
            if (best < 0 || n > bestNear) best = v, bestNear = n;
        }
        c.ok = true;
        c.p = m.verts[best];
        c.snap = Snap::Point;
        return c;
    }
    const std::vector<Edge> edges = edgesAt(m, g, sx, sy, kSnapPx);
    if (edges.empty()) return c;
    const Edge ed = edges.front();
    const Vec3 a = m.verts[ed.first], b = m.verts[ed.second];
    double mx, my;
    if (g.project((a + b) * 0.5, mx, my) && std::hypot(mx - sx, my - sy) <= kSnapPx) {
        c.ok = true;
        c.p = (a + b) * 0.5;
        c.snap = Snap::Midpoint;
        return c;
    }
    // Nearest point of the edge to the cursor, on screen.
    double ax, ay, bx, by;
    if (!g.project(a, ax, ay) || !g.project(b, bx, by)) return c;
    const double dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy;
    if (l2 < 1e-9) return c;
    const double t = std::clamp(((sx - ax) * dx + (sy - ay) * dy) / l2, 0.0, 1.0);
    c.ok = true;
    c.p = clean(a + (b - a) * t);
    c.snap = Snap::Edge;
    return c;
}

void CutTool::press(ToolEnv& env, const Pointer& p) {
    const Mesh& m = env.doc.mesh;
    if (mode_ == Mode::Loop) {
        const std::vector<Edge> edges = edgesAt(m, p.g, p.sx, p.sy, kSnapPx);
        if (edges.empty()) return;
        LoopRing r = loopRing(m, edges.front());
        if (!r.valid()) {
            why_ = "no ring of four-cornered faces goes through that edge";
            env.touch();
            return;
        }
        picked_ = r;
        why_.clear();
        env.touch();
        return;
    }
    if (!knife_.empty() && p.view != view_) return;
    const Cursor c = snapAt(m, p.g, p.sx, p.sy);
    if (!c.ok) return;
    if (knife_.empty()) view_ = p.view;
    if (!knife_.empty() && length(c.p - knife_.back()) < 1e-3) return;
    knife_.push_back(c.p);
    why_.clear();
    cursor_ = c;
    cursor_.view = p.view;
    env.touch();
}

void CutTool::hover(ToolEnv& env, const Pointer& p) {
    const Mesh& m = env.doc.mesh;
    if (mode_ == Mode::Loop) {
        const std::vector<Edge> edges = edgesAt(m, p.g, p.sx, p.sy, kSnapPx);
        LoopRing r;
        if (!edges.empty()) r = loopRing(m, edges.front());
        if (r.faces != hoverRing_.faces || r.rails != hoverRing_.rails) {
            hoverRing_ = r;
            env.touch();
        }
        return;
    }
    if (!knife_.empty() && p.view != view_) return;
    Cursor c = snapAt(m, p.g, p.sx, p.sy);
    c.view = p.view;
    if (c.ok != cursor_.ok || c.snap != cursor_.snap || c.view != cursor_.view || length(c.p - cursor_.p) > 1e-9) {
        cursor_ = c;
        env.touch();
    }
}

bool CutTool::apply(ToolEnv& env) {
    if (mode_ == Mode::Loop) {
        if (!picked_.valid()) return false;
        const LoopRing r = picked_;
        const int cuts = cuts_;
        const double pos = position_;
        const bool ok = env.doc.apply("Loop cut", [&](MeshEdit& e, Selection& sel) {
            std::set<Edge> made;
            if (!loopCut(e, r, cuts, pos, &made)) return false;
            sel = Selection();
            sel.mode = SelectMode::Edge;
            sel.edges = made;
            return true;
        });
        if (!ok) {
            why_ = "the ring can't be cut there";
            env.touch();
            return false;
        }
        picked_ = LoopRing{};
        hoverRing_ = LoopRing{};
        env.touch();
        return true;
    }
    if (knife_.size() < 2) return false;
    const std::vector<Vec3> pts = knife_;
    std::string why;
    const bool ok = env.doc.apply("Knife", [&](MeshEdit& e, Selection&) { return knife(e, pts, &why); });
    if (!ok) {
        why_ = why.empty() ? "it can't be cut there" : why;
        env.touch();
        return false;
    }
    knife_.clear();
    why_.clear();
    view_ = -1;
    env.touch();
    return true;
}

bool CutTool::key(ToolEnv& env, int key, bool shift, bool cmd) {
    if (cmd || shift || !busy()) return false;
    if (key == GLFW_KEY_ENTER) {
        apply(env);
        return true;
    }
    if (key == GLFW_KEY_ESCAPE) {
        cancel(env);
        return true;
    }
    if (key == GLFW_KEY_BACKSPACE && !knife_.empty()) {
        knife_.pop_back();
        why_.clear();
        env.touch();
        return true;
    }
    return false;
}

void CutTool::cancel(ToolEnv& env) {
    knife_.clear();
    picked_ = LoopRing{};
    hoverRing_ = LoopRing{};
    why_.clear();
    view_ = -1;
    env.touch();
}

void CutTool::setMode(ToolEnv& env, Mode m) {
    if (m == mode_) return;
    cancel(env);
    mode_ = m;
}

void CutTool::setCuts(ToolEnv& env, int n) {
    cuts_ = std::clamp(n, 1, 16);
    env.touch();
}

void CutTool::setPosition(ToolEnv& env, double p) {
    position_ = std::clamp(p, 0.01, 0.99);
    env.touch();
}

void CutTool::overlay(const ToolEnv& env, const ViewGeom& g, int view, ImDrawList* dl) const {
    const Mesh& m = env.doc.mesh;
    if (mode_ == Mode::Loop) {
        const LoopRing& r = picked_.valid() ? picked_ : hoverRing_;
        if (!r.valid()) return;
        const ImU32 col = picked_.valid() ? kLineCol : kRingCol;
        for (const auto& seg : loopCutLines(m, r, picked_.valid() ? cuts_ : 1, picked_.valid() ? position_ : 0.5)) {
            ImVec2 a, b;
            if (screen(g, seg.first, a) && screen(g, seg.second, b)) dl->AddLine(a, b, col, 2.0f);
        }
        return;
    }
    const ImU32 col = why_.empty() ? kLineCol : kBadCol;
    std::vector<ImVec2> s;
    for (const Vec3& p : knife_) {
        ImVec2 q;
        if (screen(g, p, q)) s.push_back(q);
    }
    if (s.size() >= 2) dl->AddPolyline(s.data(), (int)s.size(), col, 0, 2.0f);
    for (const ImVec2& q : s) dl->AddRectFilled({q.x - 2.5f, q.y - 2.5f}, {q.x + 2.5f, q.y + 2.5f}, col);
    if (cursor_.ok && view == cursor_.view) {
        ImVec2 q;
        if (screen(g, cursor_.p, q)) {
            if (!s.empty() && view == view_) dl->AddLine(s.back(), q, col, 1.5f);
            const ImU32 c = kSnapCol[(int)cursor_.snap];
            if (cursor_.snap == Snap::Edge) dl->AddRectFilled({q.x - 4, q.y - 4}, {q.x + 4, q.y + 4}, c);
            else dl->AddCircleFilled(q, 5, c);
            dl->AddCircle(q, 5.5f, IM_COL32(255, 255, 255, 255), 0, 1.0f);
        }
    }
}

void CutTool::properties(ToolEnv& env) {
    ImGui::SeparatorText("Cut");
    int m = (int)mode_;
    bool ch = ImGui::RadioButton("Knife", &m, 0);
    ImGui::SameLine();
    ch |= ImGui::RadioButton("Loop cut", &m, 1);
    if (ch) setMode(env, (Mode)m);
    if (mode_ == Mode::Loop) {
        int n = cuts_;
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7);
        if (ImGui::InputInt("Cuts", &n)) setCuts(env, n);
        ImGui::BeginDisabled(cuts_ != 1);
        double pct = position_ * 100;
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7);
        if (ImGui::InputDouble("Position (%)", &pct, 0, 0, "%g")) setPosition(env, pct / 100.0);
        ImGui::EndDisabled();
        if (cuts_ != 1) dimWrapped("Several cuts are spread evenly.");
        if (!picked_.valid()) dimWrapped("Point at an edge to see its ring; click to pick it. The ring stops at triangles.");
    } else if (knife_.empty()) {
        dimWrapped("Click points on edges or corners; two in a row must be on the same face. It can go round corners "
                   "onto other faces. Return cuts.");
    }
    if (!why_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.2f, 0.2f, 1));
        ImGui::TextWrapped("Can't cut: %s.", why_.c_str());
        ImGui::PopStyleColor();
    }
    if (busy()) {
        if (ImGui::Button("Cut (Return)")) apply(env);
        ImGui::SameLine();
        if (ImGui::Button("Cancel (Esc)")) cancel(env);
    }
}

} // namespace mazz
