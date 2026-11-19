// Typed dialogs: Transform (move / rotate / scale / copy), Move points, Mirror, Flatten,
// Model pivot, and the Reference images window. Each dialog applies on Return (or its
// button) as one undo step and closes on Esc.
#include "app/editor.hpp"
#include "ui/theme.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace mazz {

namespace {
const char* kAxisNames[3] = {"X", "Y", "Z"};

bool returnPressed() { return ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false); }
bool escPressed() { return ImGui::IsKeyPressed(ImGuiKey_Escape, false); }

// Three typed numbers on one line.
bool vec3Input(const char* label, Vec3& v, const char* fmt = "%g") {
    double a[3] = {v.x, v.y, v.z};
    ImGui::SetNextItemWidth(std::max(180.0f, ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(label).x - 12));
    const bool changed = ImGui::InputScalarN(label, ImGuiDataType_Double, a, 3, nullptr, nullptr, fmt);
    if (changed) v = {a[0], a[1], a[2]};
    return changed;
}

void axisRadio(const char* id, int& axis) {
    ImGui::PushID(id);
    for (int a = 0; a < 3; ++a) {
        if (a) ImGui::SameLine();
        ImGui::RadioButton(kAxisNames[a], &axis, a);
    }
    ImGui::PopID();
}

// Return applies, Esc cancels; both buttons too. Returns 1 apply, -1 cancel, 0 nothing.
int applyCancel(const char* applyLabel) {
    ImGui::Spacing();
    int r = 0;
    if (ImGui::Button(applyLabel) || returnPressed()) r = 1;
    ImGui::SameLine();
    if (ImGui::Button("Cancel (Esc)") || escPressed()) r = -1;
    return r;
}
} // namespace

void Editor::pivotCombo(const char* id, PivotChoice& c, Vec3& typed) {
    static const char* names[] = {"Selection centre", "Model pivot", "Origin", "Typed point"};
    ImGui::SetNextItemWidth(170);
    int k = (int)c;
    if (ImGui::Combo(id, &k, names, 4)) c = (PivotChoice)k;
    if (c == PivotChoice::Typed) {
        ImGui::PushID(id);
        vec3Input("cm##pt", typed);
        ImGui::PopID();
    }
}

// ---------------------------------------------------------------- transform

void Editor::applyTransform() {
    const SelectMode mode = mode_;
    const TransformState t = tf_;
    const Vec3 pivot = pivotPoint(t.pivot, t.typedPivot);
    const bool identity = length(t.move) == 0 && length(t.rotate) == 0 && t.scale.x == 100 && t.scale.y == 100 &&
                          t.scale.z == 100 && t.alongNormal == 0;
    if (t.copies > 0) {
        edit(t.copies == 1 ? "Copy" : "Copies", [&](Model& m, Selection& s) {
            const std::set<int> faces = (mode == SelectMode::Face || mode == SelectMode::Piece)
                                            ? s.faces
                                            : facesFullySelected(m.mesh, selectedVerts(m.mesh, s, mode));
            if (faces.empty()) return false;
            CloneParams p;
            p.count = t.copies;
            p.offset = t.move;
            p.offsetIsTotal = t.offsetIsTotal;
            p.rotate = t.rotate;
            p.scale = t.scale;
            p.scaleOffset = t.scaleOffset;
            p.pivot = pivot;
            Selection made;
            made.faces = cloneFaces(m.mesh, faces, p);
            s = convertSelection(m.mesh, made, SelectMode::Face, mode);
            return true;
        });
        return;
    }
    if (identity) return;
    edit("Transform", [&](Model& m, Selection& s) {
        const std::set<int> verts = selectedVerts(m.mesh, s, mode);
        if (verts.empty()) return false;
        const Affine a = about(scaling(t.scale / 100.0).then(rotationXYZ(t.rotate)), pivot).then(translation(t.move));
        transformPoints(m.mesh, verts, a);
        if (t.alongNormal != 0) moveAlongNormal(m.mesh, facesOfSelection(m.mesh, s, mode), t.alongNormal);
        return true;
    });
}

void Editor::transformDialog() {
    if (transformOpen_) {
        ImGui::OpenPopup("Move / rotate / scale / copy");
        transformOpen_ = false;
    }
    ImGui::SetNextWindowSize(ImVec2(440, 0), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Move / rotate / scale / copy", nullptr, ImGuiWindowFlags_NoResize)) return;
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    TransformState& t = tf_;
    vec3Input("Move (cm)", t.move);
    vec3Input("Rotate (degrees)", t.rotate);
    vec3Input("Scale (%)", t.scale);
    const bool faces = mode_ == SelectMode::Face || mode_ == SelectMode::Piece;
    ImGui::BeginDisabled(!faces || t.copies > 0);
    ImGui::SetNextItemWidth(120);
    ImGui::InputDouble("Along the faces' normal (cm)", &t.alongNormal, 0, 0, "%g");
    ImGui::EndDisabled();
    ImGui::SeparatorText("Around");
    pivotCombo("##tfpivot", t.pivot, t.typedPivot);
    ImGui::SeparatorText("Copies");
    ImGui::SetNextItemWidth(120);
    if (ImGui::InputInt("Copies", &t.copies)) t.copies = std::clamp(t.copies, 0, 999);
    ImGui::TextDisabled("0 changes the selection itself; otherwise each copy is moved,\nturned and scaled once more than the one before.");
    ImGui::BeginDisabled(t.copies == 0);
    ImGui::Checkbox("Move is for all copies together", &t.offsetIsTotal);
    ImGui::Checkbox("Scale the step with each copy", &t.scaleOffset);
    ImGui::EndDisabled();
    const int r = applyCancel("Apply (Return)");
    if (r == 1) applyTransform();
    if (r != 0) ImGui::CloseCurrentPopup();
    ImGui::SameLine();
    if (ImGui::Button("Reset values")) t = TransformState{};
    ImGui::EndPopup();
}

// ---------------------------------------------------------------- move points

void Editor::applyMovePoints() {
    const SelectMode mode = mode_;
    const Vec3 d = movePoints_;
    const bool absolute = movePointsAbsolute_;
    edit("Move points", [&](Model& m, Selection& s) {
        const std::set<int> verts = selectedVerts(m.mesh, s, mode);
        if (verts.empty()) return false;
        if (absolute) {
            // Moves the selection so its centre lands on the typed point.
            Vec3 mn = m.mesh.verts[*verts.begin()], mx = mn;
            for (int i : verts)
                for (int a = 0; a < 3; ++a) {
                    mn[a] = std::min(mn[a], m.mesh.verts[i][a]);
                    mx[a] = std::max(mx[a], m.mesh.verts[i][a]);
                }
            transformPoints(m.mesh, verts, translation(d - (mn + mx) / 2));
        } else {
            if (length(d) == 0) return false;
            transformPoints(m.mesh, verts, translation(d));
        }
        return true;
    });
}

void Editor::movePointsDialog() {
    if (movePointsOpen_) {
        ImGui::OpenPopup("Move points");
        movePointsOpen_ = false;
        if (movePointsAbsolute_) {
            Vec3 mn, mx;
            if (selectionBounds(mn, mx)) movePoints_ = (mn + mx) / 2;
        }
    }
    ImGui::SetNextWindowSize(ImVec2(380, 0), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Move points", nullptr, ImGuiWindowFlags_NoResize)) return;
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    vec3Input(movePointsAbsolute_ ? "Centre at (cm)" : "Move by (cm)", movePoints_);
    if (ImGui::Checkbox("Type where the selection's centre goes", &movePointsAbsolute_)) {
        if (movePointsAbsolute_) {
            Vec3 mn, mx;
            if (selectionBounds(mn, mx)) movePoints_ = (mn + mx) / 2;
        } else {
            movePoints_ = {};
        }
    }
    const int r = applyCancel("Move (Return)");
    if (r == 1) applyMovePoints();
    if (r != 0) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

// ---------------------------------------------------------------- mirror

void Editor::applyMirror() {
    const SelectMode mode = mode_;
    const MirrorState ms = mirror_;
    const Vec3 pivot = pivotPoint(ms.pivot, ms.typedPivot);
    edit(ms.copy ? "Mirror copy" : "Mirror", [&](Model& m, Selection& s) {
        const std::set<int> faces = facesOfSelection(m.mesh, s, mode);
        if (faces.empty()) return false;
        Selection out;
        out.faces = mirrorFaces(m.mesh, faces, ms.axis, pivot, ms.copy);
        s = convertSelection(m.mesh, out, SelectMode::Face, mode);
        return true;
    });
}

void Editor::mirrorDialog() {
    if (mirrorOpen_) {
        ImGui::OpenPopup("Mirror");
        mirrorOpen_ = false;
    }
    ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Mirror", nullptr, ImGuiWindowFlags_NoResize)) return;
    ImGui::TextUnformatted("Flip along");
    ImGui::SameLine();
    axisRadio("mirroraxis", mirror_.axis);
    ImGui::TextUnformatted("Across");
    ImGui::SameLine();
    pivotCombo("##mirrorpivot", mirror_.pivot, mirror_.typedPivot);
    ImGui::Checkbox("Keep the original (mirrored copy)", &mirror_.copy);
    const int r = applyCancel("Mirror (Return)");
    if (r == 1) applyMirror();
    if (r != 0) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

// ---------------------------------------------------------------- flatten

void Editor::applyFlatten() {
    const SelectMode mode = mode_;
    const FlattenState fs = flatten_;
    Vec3 mn, mx;
    if (!selectionBounds(mn, mx)) return;
    const int a = std::clamp(fs.axis, 0, 2);
    const double value = fs.where == 0 ? mn[a] : fs.where == 1 ? (mn[a] + mx[a]) / 2 : fs.where == 2 ? mx[a] : fs.typed;
    edit("Flatten", [&](Model& m, Selection& s) {
        const std::set<int> verts = selectedVerts(m.mesh, s, mode);
        if (verts.empty()) return false;
        bool any = false;
        for (int i : verts) any = any || m.mesh.verts[i][a] != value;
        if (!any) return false;
        flattenPoints(m.mesh, verts, a, value);
        return true;
    });
}

void Editor::flattenDialog() {
    if (flattenOpen_) {
        ImGui::OpenPopup("Flatten");
        flattenOpen_ = false;
    }
    ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Flatten", nullptr, ImGuiWindowFlags_NoResize)) return;
    ImGui::TextUnformatted("Line the points up on");
    ImGui::SameLine();
    axisRadio("flattenaxis", flatten_.axis);
    ImGui::RadioButton("Lowest", &flatten_.where, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Middle", &flatten_.where, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Highest", &flatten_.where, 2);
    ImGui::SameLine();
    ImGui::RadioButton("At", &flatten_.where, 3);
    ImGui::SameLine();
    ImGui::BeginDisabled(flatten_.where != 3);
    ImGui::SetNextItemWidth(90);
    ImGui::InputDouble("cm##flat", &flatten_.typed, 0, 0, "%g");
    ImGui::EndDisabled();
    const int r = applyCancel("Flatten (Return)");
    if (r == 1) applyFlatten();
    if (r != 0) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

// ---------------------------------------------------------------- pivot

void Editor::pivotDialog() {
    if (!pivotOpen_) return;
    // Not modal: "Place by clicking" needs the views.
    ImGui::SetNextWindowSize(ImVec2(380, 0), ImGuiCond_Appearing);
    {
        // Over the texture view side, so the 2D views stay free for clicking.
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos({vp->WorkPos.x + vp->WorkSize.x * 0.81f - 10, vp->WorkPos.y + vp->WorkSize.y - 20},
                                ImGuiCond_FirstUseEver, {1, 1});
    }
    bool open = true;
    if (ImGui::Begin("Model pivot", &open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking)) {
        ImGui::TextDisabled("The point the model is placed and turned by in Scalpellino.\nExports put it at 0, 0, 0.");
        vec3Input("cm##pivot", pivotTyped_);
        auto setPivot = [this](const Vec3& p) {
            pivotTyped_ = p;
            edit("Move pivot", [p](Model& m, Selection&) {
                if (length(m.pivot - p) == 0) return false;
                m.pivot = p;
                return true;
            });
        };
        if (ImGui::Button("Set (Return)") || (ImGui::IsWindowFocused() && returnPressed())) setPivot(pivotTyped_);
        ImGui::SameLine();
        if (ImGui::Button("Origin")) setPivot({});
        ImGui::SameLine();
        Vec3 mn, mx;
        const bool haveModel = doc_.model.mesh.bounds(mn, mx);
        ImGui::BeginDisabled(!haveModel);
        if (ImGui::Button("Bottom centre")) setPivot({(mn.x + mx.x) / 2, (mn.y + mx.y) / 2, mn.z});
        ImGui::EndDisabled();
        ImGui::SameLine();
        Vec3 smn, smx;
        const bool haveSel = selectionBounds(smn, smx);
        ImGui::BeginDisabled(!haveSel);
        if (ImGui::Button("Selection centre")) setPivot((smn + smx) / 2);
        ImGui::EndDisabled();
        ImGui::Checkbox("Place by clicking in a 2D view", &pivotPlacing_);
        if (pivotPlacing_) ImGui::TextDisabled("Click sets the two axes that view shows (snaps to the grid).");
        if (ImGui::IsWindowFocused() && escPressed()) open = false;
    }
    ImGui::End();
    if (!open) {
        pivotOpen_ = false;
        pivotPlacing_ = false;
    }
}

// ---------------------------------------------------------------- reference images

void Editor::refsWindow() {
    if (!refsOpen_) return;
    ImGui::SetNextWindowSize(ImVec2(420, 460), ImGuiCond_FirstUseEver);
    {
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos({vp->WorkPos.x + vp->WorkSize.x * 0.81f - 10, vp->WorkPos.y + 40}, ImGuiCond_FirstUseEver, {1, 0});
    }
    if (ImGui::Begin("Reference images", &refsOpen_, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking)) {
        ImGui::TextUnformatted("View");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        if (ImGui::BeginCombo("##refdir", viewDirName((ViewDir)refDir_))) {
            for (int d = 0; d < kViewDirCount; ++d) {
                if ((ViewDir)d == ViewDir::Persp) continue;
                if (ImGui::Selectable(viewDirName((ViewDir)d), refDir_ == d)) refDir_ = d;
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Load image...")) loadRefImage();
        bool show = showRefs_;
        ImGui::SameLine();
        if (ImGui::Checkbox("Show", &show)) run(Act::ShowRefs);
        ImGui::Separator();

        int shown = 0;
        int removeAt = -1;
        for (int i = 0; i < (int)doc_.model.refs.size(); ++i) {
            if ((int)doc_.model.refs[i].dir != refDir_) continue;
            ++shown;
            ImGui::PushID(i);
            RefImage& r = doc_.model.refs[i];
            const TextureCache::Entry& e = textures_.get(resolveRefPath(r.path, modelDir()));
            ImGui::TextUnformatted(r.path.c_str());
            if (e.failed) {
                ImGui::SameLine();
                ImGui::TextColored(theme::kAccent, "(can't read it)");
            }
            // Each field edits live; letting go of it makes one undo step.
            auto track = [&](bool changed) {
                if (ImGui::IsItemActivated()) refBase_ = {doc_.model, doc_.sel};
                if (changed) touch();
                if (ImGui::IsItemDeactivatedAfterEdit()) doc_.commitFrom(refBase_, "Change reference image");
            };
            ImGui::SetNextItemWidth(150);
            double xy[2] = {r.x, r.y};
            const bool cxy = ImGui::InputScalarN("Centre (cm)", ImGuiDataType_Double, xy, 2, nullptr, nullptr, "%g");
            if (cxy) { r.x = xy[0]; r.y = xy[1]; }
            track(cxy);
            ImGui::SetNextItemWidth(150);
            const bool cw = ImGui::InputDouble("Width (cm)", &r.width, 0, 0, "%g");
            if (cw) r.width = std::max(0.1, r.width);
            track(cw);
            ImGui::SetNextItemWidth(150);
            track(ImGui::InputDouble("Rotation (degrees)", &r.rotation, 0, 0, "%g"));
            float a = (float)(r.alpha * 100);
            ImGui::SetNextItemWidth(150);
            const bool ca = ImGui::SliderFloat("Opacity", &a, 0, 100, "%.0f%%", ImGuiSliderFlags_AlwaysClamp);
            if (ca) r.alpha = a / 100.0;
            track(ca);
            int layer = r.inFront ? 1 : 0;
            if (ImGui::RadioButton("Behind the model", &layer, 0) | (ImGui::SameLine(), ImGui::RadioButton("In front", &layer, 1))) {
                const bool front = layer == 1;
                edit("Change reference image", [i, front](Model& m, Selection&) {
                    if (m.refs[i].inFront == front) return false;
                    m.refs[i].inFront = front;
                    return true;
                });
            }
            if (ImGui::SmallButton("Remove")) removeAt = i;
            ImGui::Separator();
            ImGui::PopID();
        }
        if (removeAt >= 0) {
            edit("Remove reference image", [removeAt](Model& m, Selection&) {
                m.refs.erase(m.refs.begin() + removeAt);
                return true;
            });
        }
        if (!shown) ImGui::TextDisabled("No images in this view. PNG, JPEG, TGA or BMP.");
    }
    ImGui::End();
}

} // namespace mazz
