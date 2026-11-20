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
bool nearV(const Vec3& a, const Vec3& b, double tol) { return length(a - b) <= tol; }
} // namespace

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
            // Mouse over a world point as seen in a view.
            int v = 0;
            Vec3 w;
            in >> v >> w.x >> w.y >> w.z;
            double sx = 0, sy = 0;
            if (viewIndex(v)) {
                if (!geom(v).project(w, sx, sy)) scriptFail("point not visible in that view");
                else io.AddMousePosEvent((float)sx, (float)sy);
            }
        } else if (cmd == "varrow") {
            // Mouse on the selection's arrow (axis 0/1/2) in a view, `cm` along it.
            int v = 0, axis = 0;
            double cm = 0;
            in >> v >> axis >> cm;
            // The arrow as it was when the button went down (the selection moves while
            // dragging).
            static Vec3 anchor;
            Vec3 mn, mx;
            double sx = 0, sy = 0;
            if (!io.MouseDown[0] && selectionBounds(doc_.mesh, doc_.sel, mn, mx)) anchor = (mn + mx) / 2;
            if (viewIndex(v)) {
                if (doc_.sel.empty() || !arrowPoint(geom(v), anchor, axis, cm, sx, sy)) scriptFail("no such arrow");
                else io.AddMousePosEvent((float)sx, (float)sy);
            }
        } else if (cmd == "hold" || cmd == "letgo") {
            // Modifier keys held during mouse actions: shift, cmd, alt (Option).
            std::string k;
            in >> k;
            const bool down = cmd == "hold";
            if (k == "shift") io.AddKeyEvent(ImGuiMod_Shift, down);
            else if (k == "cmd") io.AddKeyEvent(ImGuiMod_Super, down), io.AddKeyEvent(ImGuiMod_Ctrl, down);
            else if (k == "alt") io.AddKeyEvent(ImGuiMod_Alt, down);
            else scriptFail("unknown modifier " + k);
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
        } else if (cmd == "quit") {
            quit_ = true;
        } else if (cmd == "saveas" || cmd == "open") {
            // Paths relative to the script's working directory (tests run in a scratch dir).
            std::string path;
            std::getline(in >> std::ws, path);
            if (path.empty()) scriptFail(cmd + " needs a path");
            else if (cmd == "saveas") savePath(path);
            else openPath(path);
        } else if (cmd == "prim") {
            // prim <shape>: the Primitives tool with that shape (as the Create menu does).
            std::string name;
            in >> name;
            PrimKind k;
            if (!primKindFromName(name, k)) scriptFail("unknown shape " + name);
            else { primTool_.setShape(k); setTool(&primTool_); }
        } else if (cmd == "primset") {
            // primset <setting> <value>: as typed in Properties.
            std::string what;
            double val = 0;
            in >> what >> val;
            PrimParams& p = primTool_.params();
            const int iv = (int)val;
            if (what == "segx") p.segX = iv;
            else if (what == "segy") p.segY = iv;
            else if (what == "segz") p.segZ = iv;
            else if (what == "rows") p.rows = iv;
            else if (what == "cols") p.cols = iv;
            else if (what == "sides") p.sides = iv;
            else if (what == "segments") p.segments = iv;
            else if (what == "caprings") p.capRings = iv;
            else if (what == "hole") p.inner = val;
            else if (what == "tubesides") p.tubeSides = iv;
            else if (what == "tubewidth") p.tubeWidth = val;
            else if (what == "stacks") p.stacks = iv;
            else if (what == "iterations") p.iterations = iv;
            else if (what == "rounded") p.roundSegments = iv;
            else if (what == "rounding") p.roundSize = val;
            else if (what == "rockfaces") p.rock.faces = iv;
            else if (what == "roughness") p.rock.roughness = val;
            else if (what == "flatbottom") p.rock.flatBottom = iv != 0;
            else if (what == "bottom") p.rock.bottomHeight = val;
            else if (what == "quads") p.rock.joinQuads = iv != 0;
            else if (what == "seed") p.rock.seed = (unsigned)iv;
            else scriptFail("unknown setting " + what);
            p = clampParams(p);
            touch();
        } else if (cmd == "opset") {
            // opset <setting> <values>: as typed in the open operation window.
            std::string what;
            in >> what;
            auto v3 = [&](Vec3& v) { in >> v.x >> v.y >> v.z; };
            auto pivot = [&](PivotKind& k) {
                std::string w;
                in >> w;
                k = w == "origin" ? PivotKind::Origin : w == "typed" ? PivotKind::Typed : PivotKind::Centre;
            };
            if (what == "move") v3(opTransform_.move);
            else if (what == "rotate") v3(opTransform_.rotate);
            else if (what == "scale") v3(opTransform_.scale);
            else if (what == "normal") in >> opTransform_.alongNormal;
            else if (what == "pivot") pivot(opTransform_.pivot);
            else if (what == "copies") in >> opReplicate_.copies;
            else if (what == "offset") v3(opReplicate_.offset);
            else if (what == "turn") v3(opReplicate_.turn);
            else if (what == "copyscale") v3(opReplicate_.scale);
            else if (what == "scaleoffset") { int b = 0; in >> b; opReplicate_.scaleOffset = b != 0; }
            else if (what == "copypivot") pivot(opReplicate_.pivot);
            else if (what == "axis") in >> flattenAxis_;
            else if (what == "to") {
                std::string w;
                in >> w;
                flattenTo_ = w == "lowest" ? FlattenTo::Lowest : w == "highest" ? FlattenTo::Highest : FlattenTo::Average;
            } else scriptFail("unknown setting " + what);
            refreshOpPreview();
        } else if (cmd == "addbox") {
            // Test helper (until the box tool exists): adds a box as one undo step and
            // selects it.
            Vec3 a, b;
            in >> a.x >> a.y >> a.z >> b.x >> b.y >> b.z;
            doc_.apply("Box", [&](MeshEdit& e, Selection& sel) {
                Mesh box;
                addBox(box, a, b);
                const int base = (int)e.mesh().verts.size();
                for (const Vec3& p : box.verts) e.addVert(p);
                sel = Selection();
                for (Face f : box.faces) {
                    for (int k = 0; k < f.n; ++k) f.v[k] += base;
                    sel.faces.insert(e.addFace(f));
                }
                return true;
            });
            touch();
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
                if (m != selectModeName(doc_.sel.mode)) scriptFail(std::string("mode is ") + selectModeName(doc_.sel.mode));
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
            } else if (what == "dirty" || what == "undo" || what == "faces" || what == "verts" || what == "selected") {
                long want = 0;
                in >> want;
                long have = 0;
                if (what == "dirty") have = doc_.dirty() ? 1 : 0;
                else if (what == "undo") have = doc_.undoCount();
                else if (what == "faces") have = (long)doc_.mesh.faces.size();
                else if (what == "verts") have = (long)doc_.mesh.verts.size();
                else have = (long)(doc_.sel.mode == SelectMode::Vert ? doc_.sel.verts.size()
                                   : doc_.sel.mode == SelectMode::Edge ? doc_.sel.edges.size()
                                                                       : doc_.sel.faces.size());
                if (have != want) scriptFail(what + " is " + std::to_string(have));
            } else if (what == "bounds" || what == "selbounds") {
                Vec3 a, b, mn, mx;
                in >> a.x >> a.y >> a.z >> b.x >> b.y >> b.z;
                const bool have = what == "bounds" ? doc_.mesh.bounds(mn, mx) : selectionBounds(doc_.mesh, doc_.sel, mn, mx);
                if (!have || !nearV(mn, a, 1e-6) || !nearV(mx, b, 1e-6)) {
                    char buf[200];
                    if (have) std::snprintf(buf, sizeof buf, "%s are %g %g %g  %g %g %g", what.c_str(), mn.x, mn.y, mn.z, mx.x, mx.y, mx.z);
                    else std::snprintf(buf, sizeof buf, "%s: nothing there", what.c_str());
                    scriptFail(buf);
                }
            } else if (what == "previewfaces") {
                int n = 0;
                in >> n;
                if (primTool_.previewFaces() != n) scriptFail("preview has " + std::to_string(primTool_.previewFaces()) + " faces");
            } else if (what == "preview") {
                int b = 0;
                in >> b;
                if (primTool_.hasPreview() != (b != 0)) scriptFail(std::string("preview is ") + (primTool_.hasPreview() ? "1" : "0"));
            } else if (what == "op") {
                std::string w;
                in >> w;
                const char* names[] = {"none", "transform", "replicate", "flatten"};
                if (w != names[(int)op_]) scriptFail(std::string("operation window is ") + names[(int)op_]);
            } else if (what == "tool") {
                std::string t;
                in >> t;
                if (t != tool_->name()) scriptFail(std::string("tool is ") + tool_->name());
            } else if (what == "title") {
                std::string t;
                std::getline(in >> std::ws, t);
                if (title_ != t) scriptFail("title is \"" + title_ + "\"");
            } else if (what == "fileline") {
                // expect fileline <path> <exact line>: the file contains that line.
                std::string path, want;
                in >> path;
                std::getline(in >> std::ws, want);
                std::ifstream f(path);
                std::string l;
                bool found = false;
                while (f && std::getline(f, l))
                    if (l == want) found = true;
                if (!found) scriptFail(path + " has no line \"" + want + "\"");
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
