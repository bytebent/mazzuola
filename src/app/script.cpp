// Scripted input for automated UI tests and screenshots (tests/ui/*.script).
// One command per frame (wait N skips frames). Mouse input goes through ImGui's IO
// queue, keys through Editor::keyPressed: the same paths as real input.
// The command list is in docs/DEV.md ("Script language"); keep it in sync.
#include "app/editor.hpp"
#include <GLFW/glfw3.h>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace mazz {

void Editor::loadScript(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::fprintf(stderr, "Mazzuola: cannot open script %s\n", path.c_str());
        ++scriptFailures_;
        quit_ = true;
        return;
    }
    std::string line;
    while (std::getline(f, line)) script_.push_back(line);
    // Start after a few frames so the dock layout has settled.
    scriptWait_ = 3;
}

void Editor::scriptFail(const std::string& msg) {
    ++scriptFailures_;
    std::fprintf(stderr, "FAIL script: line %d: %s\n", scriptLine_, msg.c_str());
}

namespace {
int buttonFromName(const std::string& s) {
    if (s == "left") return 0;
    if (s == "right") return 1;
    if (s == "middle") return 2;
    return -1;
}
bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }
} // namespace

// Paths starting with "@/" are relative to the script's folder (test data).
static std::string scriptPath(const std::string& p, const std::string& script) {
    if (p.rfind("@/", 0) != 0) return p;
    const size_t slash = script.find_last_of('/');
    return (slash == std::string::npos ? std::string(".") : script.substr(0, slash)) + p.substr(1);
}

void Editor::stepScript() {
    if (scriptWait_ > 0) { --scriptWait_; return; }
    ImGuiIO& io = ImGui::GetIO();
    while (!script_.empty()) {
        std::string line = script_.front();
        script_.pop_front();
        ++scriptLine_;
        if (const size_t hash = line.find('#'); hash != std::string::npos) line.resize(hash);
        std::istringstream in(line);
        std::string cmd;
        if (!(in >> cmd)) continue; // blank or comment: no frame used

        auto viewIndex = [&](int v) {
            if (v < 0 || v > 3) { scriptFail("bad view index"); return false; }
            return true;
        };

        if (cmd == "wait") {
            int n = 1;
            in >> n;
            scriptWait_ = std::max(0, n - 1);
        } else if (cmd == "key") {
            std::string k;
            in >> k;
            Chord c;
            if (!parseChord(k, c)) scriptFail("unknown key " + k);
            else keyPressed(c.key, c.shift, c.cmd);
        } else if (cmd == "action") {
            std::string id;
            in >> id;
            Act a;
            if (!actionFromId(id, a)) scriptFail("unknown action " + id);
            else run(a);
        } else if (cmd == "vmove") {
            int v = 0;
            double fx = 0.5, fy = 0.5;
            in >> v >> fx >> fy;
            if (viewIndex(v))
                io.AddMousePosEvent(viewImgPos_[v].x + (float)(fx * viewImgSize_[v].x),
                                    viewImgPos_[v].y + (float)(fy * viewImgSize_[v].y));
        } else if (cmd == "vworld") {
            // vworld <view> <x> <y> <z>: the mouse to where that world point shows in the view
            int v = 0;
            Vec3 p;
            in >> v >> p.x >> p.y >> p.z;
            double x, y, d;
            if (!viewIndex(v)) {
            } else if (!projector(v).project(p, x, y, d)) scriptFail("point is not in the view");
            else io.AddMousePosEvent(viewImgPos_[v].x + (float)x, viewImgPos_[v].y + (float)y);
        } else if (cmd == "nudge") {
            // nudge <dx> <dy>: moves the mouse by logical pixels
            float dx = 0, dy = 0;
            in >> dx >> dy;
            io.AddMousePosEvent(io.MousePos.x + dx, io.MousePos.y + dy);
        } else if (cmd == "press" || cmd == "release") {
            std::string b;
            in >> b;
            const int btn = buttonFromName(b);
            if (btn < 0) scriptFail("unknown button " + b);
            else io.AddMouseButtonEvent(btn, cmd == "press");
        } else if (cmd == "wheel") {
            float d = 0;
            in >> d;
            io.AddMouseWheelEvent(0, d);
        } else if (cmd == "spacedown" || cmd == "spaceup") {
            io.AddKeyEvent(ImGuiKey_Space, cmd == "spacedown");
        } else if (cmd == "uikey") {
            std::string k;
            in >> k;
            const ImGuiKey key = k == "Escape" ? ImGuiKey_Escape : k == "Enter" ? ImGuiKey_Enter : ImGuiKey_None;
            if (key == ImGuiKey_None) scriptFail("unknown ui key " + k);
            else { io.AddKeyEvent(key, true); io.AddKeyEvent(key, false); }
        } else if (cmd == "type") {
            std::string t;
            std::getline(in >> std::ws, t);
            io.AddInputCharactersUTF8(t.c_str());
        } else if (cmd == "viewdir") {
            int v = 0;
            std::string name;
            in >> v >> name;
            ViewDir d;
            if (!viewIndex(v)) {
            } else if (!viewDirFromName(name.c_str(), d)) scriptFail("unknown view direction " + name);
            else views_[v].setDir(d);
        } else if (cmd == "maximise") {
            int v = 0;
            in >> v;
            if (viewIndex(v)) { layout_.maximised = v; markViewsDirty(); }
        } else if (cmd == "restore") {
            layout_.maximised = -1;
            markViewsDirty();
        } else if (cmd == "grid") {
            double s = 8;
            in >> s;
            grid_.setSize(s);
            markViewsDirty();
        } else if (cmd == "camera") {
            int v = 0;
            Camera3D c;
            in >> v >> c.yaw >> c.pitch >> c.distance >> c.target.x >> c.target.y >> c.target.z;
            if (viewIndex(v)) { views_[v].cam3d = c; views_[v].dirty = true; }
        } else if (cmd == "flymove") {
            double f = 0, r = 0, u = 0;
            in >> f >> r >> u;
            if (!flying_ || flyView_ < 0) scriptFail("flymove while not flying");
            else { views_[flyView_].cam3d.fly(f, r, u); views_[flyView_].dirty = true; }
        } else if (cmd == "shot") {
            std::string file;
            in >> file;
            screenshotRequest_ = file;
        } else if (cmd == "paintshot") {
            in >> paintShot_;
        } else if (cmd == "open") {
            std::string f;
            in >> f;
            f = scriptPath(f, opt_.scriptPath);
            if (!openPath(f)) scriptFail("cannot open " + f);
        } else if (cmd == "saveas") {
            std::string f;
            in >> f;
            if (!savePath(f)) scriptFail("cannot save " + f);
        } else if (cmd == "export") {
            std::string f;
            in >> f;
            if (!exportPath(f)) scriptFail("cannot export " + f);
        } else if (cmd == "new") {
            fileNew();
        } else if (cmd == "prim") {
            std::string k;
            in >> k;
            PrimKind pk;
            if (!primKindFromName(k, pk)) scriptFail("unknown primitive " + k);
            else startPrimitive(pk);
        } else if (cmd == "primset") {
            std::string key;
            in >> key;
            double a = 0, b = 0, c = 0;
            in >> a;
            if (key == "size") { in >> b >> c; prim_.size = {a, b, c}; }
            else if (key == "centre") { in >> b >> c; prim_.centre = {a, b, c}; }
            else if (key == "axis") prim_.axis = (int)a;
            else if (key == "sides") prim_.sides = (int)a;
            else if (key == "stacks") prim_.stacks = (int)a;
            else if (key == "segs") { in >> b >> c; prim_.segX = (int)a; prim_.segY = (int)b; prim_.segZ = (int)c; }
            else if (key == "rows") { in >> b; prim_.rows = (int)a; prim_.cols = (int)b; }
            else if (key == "faces") prim_.faces = (int)a;
            else if (key == "seed") prim_.seed = (uint32_t)a;
            else if (key == "iterations") prim_.iterations = (int)a;
            else if (key == "rings") prim_.rings = (int)a;
            else if (key == "round") prim_.round = a;
            else { scriptFail("unknown primitive setting " + key); }
            prim_ = clampParams(prim_);
            if (primActive_) updatePrimPreview();
        } else if (cmd == "ref") {
            // ref <view direction> <image path> <width cm>
            std::string dir, file;
            double width = 100;
            in >> dir >> file >> width;
            file = scriptPath(file, opt_.scriptPath);
            ViewDir d;
            if (!viewDirFromName(dir.c_str(), d) || d == ViewDir::Persp) scriptFail("bad reference direction " + dir);
            else {
                RefImage r;
                r.dir = d;
                r.path = file;
                r.width = width;
                edit("Add reference image", [&](Model& m, Selection&) { m.refs.push_back(r); return true; });
            }
        } else if (cmd == "dialog") {
            // Opens a dialog the way its menu item does (the dialogs then take typed input).
            std::string which;
            in >> which;
            if (which == "pivotplace") { pivotOpen_ = true; pivotPlacing_ = true; }
            else scriptFail("unknown dialog " + which);
        } else if (cmd == "transform") {
            // transform move x y z | rotate x y z | scale x y z | normal d | copies n | apply
            std::string what;
            in >> what;
            if (what == "move") in >> tf_.move.x >> tf_.move.y >> tf_.move.z;
            else if (what == "rotate") in >> tf_.rotate.x >> tf_.rotate.y >> tf_.rotate.z;
            else if (what == "scale") in >> tf_.scale.x >> tf_.scale.y >> tf_.scale.z;
            else if (what == "normal") in >> tf_.alongNormal;
            else if (what == "copies") in >> tf_.copies;
            else if (what == "apply") applyTransform();
            else if (what == "reset") tf_ = TransformState{};
            else scriptFail("unknown transform setting " + what);
        } else if (cmd == "mirror") {
            int axis = 0, copy = 0;
            in >> axis >> copy;
            mirror_.axis = axis;
            mirror_.copy = copy != 0;
            applyMirror();
        } else if (cmd == "flatten") {
            int axis = 2, where = 0;
            double at = 0;
            in >> axis >> where >> at; // flatten <axis> <0 lowest|1 middle|2 highest|3 at> [cm]
            flatten_.axis = axis;
            flatten_.where = where;
            flatten_.typed = at;
            applyFlatten();
        } else if (cmd == "quit") {
            quit_ = true;
        } else if (cmd == "expect") {
            std::string what;
            in >> what;
            if (what == "grid") {
                double s = 0;
                in >> s;
                if (!near(grid_.size, s, 1e-9)) scriptFail("grid is " + std::to_string(grid_.size));
            } else if (what == "gridvisible" || what == "snap") {
                int b = 0;
                in >> b;
                const bool val = what == "snap" ? grid_.snap : grid_.visible;
                if (val != (b != 0)) scriptFail(what + " is " + (val ? "1" : "0"));
            } else if (what == "mode") {
                std::string m;
                in >> m;
                if (m != selectModeName(mode_)) scriptFail(std::string("mode is ") + selectModeName(mode_));
            } else if (what == "viewdir") {
                int v = 0;
                std::string name;
                in >> v >> name;
                if (viewIndex(v) && name != viewDirName(views_[v].dir))
                    scriptFail(std::string("view direction is ") + viewDirName(views_[v].dir));
            } else if (what == "maximised") {
                int v = 0;
                in >> v;
                if (layout_.maximised != v) scriptFail("maximised is " + std::to_string(layout_.maximised));
            } else if (what == "flying") {
                int b = 0;
                in >> b;
                if (flying_ != (b != 0)) scriptFail(std::string("flying is ") + (flying_ ? "1" : "0"));
            } else if (what == "paintwindow") {
                int b = 0;
                in >> b;
                if (paintOpen_ != (b != 0)) scriptFail(std::string("paint window open is ") + (paintOpen_ ? "1" : "0"));
            } else if (what == "controls") {
                int b = 0;
                in >> b;
                if (controlsOpen_ != (b != 0)) scriptFail("controls window state differs");
            } else if (what == "cam") {
                int v = 0;
                double yaw = 0, pitch = 0, dist = 0;
                in >> v >> yaw >> pitch >> dist;
                if (viewIndex(v)) {
                    const Camera3D& c = views_[v].cam3d;
                    if (!near(c.yaw, yaw, 0.5) || !near(c.pitch, pitch, 0.5) || !near(c.distance, dist, 0.5)) {
                        char b[160];
                        std::snprintf(b, sizeof b, "camera is yaw %.2f pitch %.2f distance %.2f", c.yaw, c.pitch, c.distance);
                        scriptFail(b);
                    }
                }
            } else if (what == "ortho") {
                int v = 0;
                double cu = 0, cv = 0, zoom = 0;
                in >> v >> cu >> cv >> zoom;
                if (viewIndex(v)) {
                    const OrthoCamera& o = views_[v].ortho;
                    const double tol = std::max(0.5, 0.01 * std::max(std::fabs(cu), std::fabs(cv)));
                    if (!near(o.cu, cu, tol) || !near(o.cv, cv, tol) || !near(o.zoom, zoom, zoom * 0.01)) {
                        char b[160];
                        std::snprintf(b, sizeof b, "view is centre %.2f %.2f zoom %.4f", o.cu, o.cv, o.zoom);
                        scriptFail(b);
                    }
                }
            } else if (what == "faces" || what == "verts" || what == "tris" || what == "selected" || what == "refs" ||
                       what == "dirty" || what == "hidden" || what == "undo") {
                long n = 0;
                in >> n;
                const Mesh& m = doc_.model.mesh;
                long have = 0;
                if (what == "faces") have = (long)m.faces.size();
                else if (what == "verts") have = (long)m.verts.size();
                else if (what == "tris") have = m.triangleCount();
                else if (what == "refs") have = (long)doc_.model.refs.size();
                else if (what == "dirty") have = doc_.dirty ? 1 : 0;
                else if (what == "undo") have = doc_.canUndo() ? 1 : 0;
                else if (what == "hidden") { for (const Face& f : m.faces) have += f.hidden ? 1 : 0; }
                else {
                    const Selection& sel = doc_.sel;
                    have = mode_ == SelectMode::Vertex ? (long)sel.verts.size()
                         : mode_ == SelectMode::Edge   ? (long)sel.edges.size()
                                                       : (long)sel.faces.size();
                }
                if (have != n) scriptFail(what + " is " + std::to_string(have));
            } else if (what == "bounds") {
                Vec3 a, b, mn, mx;
                in >> a.x >> a.y >> a.z >> b.x >> b.y >> b.z;
                if (!doc_.model.mesh.bounds(mn, mx)) scriptFail("model is empty");
                else if (length(mn - a) > 0.01 || length(mx - b) > 0.01) {
                    char t[200];
                    std::snprintf(t, sizeof t, "bounds are %g %g %g  %g %g %g", mn.x, mn.y, mn.z, mx.x, mx.y, mx.z);
                    scriptFail(t);
                }
            } else if (what == "pivot") {
                Vec3 p;
                in >> p.x >> p.y >> p.z;
                if (length(p - doc_.model.pivot) > 0.01) {
                    char t[120];
                    std::snprintf(t, sizeof t, "pivot is %g %g %g", doc_.model.pivot.x, doc_.model.pivot.y, doc_.model.pivot.z);
                    scriptFail(t);
                }
            } else if (what == "tool") {
                std::string t;
                in >> t;
                if (t != toolName(tool_)) scriptFail(std::string("tool is ") + toolName(tool_));
            } else if (what == "preview") {
                int b = 0;
                in >> b;
                if (primActive_ != (b != 0)) scriptFail(std::string("preview is ") + (primActive_ ? "1" : "0"));
            } else if (what == "handles") {
                std::string h;
                in >> h;
                const char* have = handleMode_ == HandleMode::Scale ? "scale" : "rotate";
                if (h != have) scriptFail(std::string("handles are ") + have);
            } else {
                scriptFail("unknown expectation " + what);
            }
        } else {
            scriptFail("unknown command " + cmd);
        }
        return; // one command per frame
    }
    // Script finished. Without a `quit` the app stays open and goes idle (used by the
    // idle CPU check); a failed expect still quits, so a broken run ends.
    scriptDone_ = true;
    if (scriptFailures_ > 0 && screenshotRequest_.empty()) quit_ = true;
}

} // namespace mazz
