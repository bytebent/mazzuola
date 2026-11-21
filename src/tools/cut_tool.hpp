#pragma once
// Cut tool (Shift+X), ARCHITECTURE §6 Cut (v0.2.4, agreed 2026-11-21):
//  - Knife: click points on edges or corners (green a point, cyan a midpoint, red on an
//    edge); two clicks in a row must share a face, which is cut between them; it may go
//    round corners onto other faces. Return cuts, Esc cancels, Backspace takes a point
//    back.
//  - Loop cut: hover an edge to see the ring of cuts through the quads in line with
//    it; click to pick it; typed number of cuts (1) and position (50 %); Return applies
//    (the new edges are selected), Esc drops it.
#include "core/divide_ops.hpp"
#include "tools/tool.hpp"
#include <string>
#include <vector>

namespace mazz {

class CutTool : public Tool {
public:
    enum class Mode { Knife, Loop };

    const char* name() const override { return "Cut"; }
    const char* hint() const override;
    void press(ToolEnv& env, const Pointer& p) override;
    void hover(ToolEnv& env, const Pointer& p) override;
    bool key(ToolEnv& env, int key, bool shift, bool cmd) override;
    void overlay(const ToolEnv& env, const ViewGeom& g, int view, ImDrawList* dl) const override;
    void properties(ToolEnv& env) override;
    void cancel(ToolEnv& env) override;
    bool busy() const override { return !knife_.empty() || picked_.valid(); }

    void setMode(ToolEnv& env, Mode m);
    void setCuts(ToolEnv& env, int n);
    void setPosition(ToolEnv& env, double p); // 0..1
    bool apply(ToolEnv& env);
    int knifePoints() const { return (int)knife_.size(); }
    bool ringPicked() const { return picked_.valid(); }
    int ringFaces() const { return (int)(picked_.valid() ? picked_ : hoverRing_).faces.size(); }

private:
    enum class Snap { None, Point, Midpoint, Edge };
    struct Cursor {
        bool ok = false;
        Vec3 p;
        Snap snap = Snap::None;
        int view = -1;
    };
    Mode mode_ = Mode::Knife;
    std::vector<Vec3> knife_;
    Cursor cursor_;
    std::string why_;
    int view_ = -1;
    LoopRing hoverRing_, picked_;
    int cuts_ = 1;
    double position_ = 0.5;

    Cursor snapAt(const Mesh& m, const ViewGeom& g, double sx, double sy) const;
};

} // namespace mazz
