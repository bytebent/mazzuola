// Reference images and the scale figure (ARCHITECTURE §5).
//
// Reference images: one per view direction, shared by the views facing that way, none in
// the 3D view, never saved. "Image..." in a 2D view's header loads one; it first fills
// the visible part of the view. While its window is open, the views facing its way move
// the image (drag inside it) and resize it (drag a corner, the opposite one stays,
// proportions kept) instead of using the tool; it is never picked while modelling.
//
// The scale figure: View -> Scale figure; stands beside the origin on -X, just outside
// the model's box (core/figure.hpp).
#include "app/editor.hpp"
#include "core/figure.hpp"
#include "platform/file_dialog.hpp"
#include "render/image_draw.hpp"
#include "ui/theme.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_TGA
#define STBI_ONLY_BMP
#include "stb_image.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace fs = std::filesystem;

namespace mazz {

namespace {
constexpr float kCornerGrab = 8.0f; // logical px around a corner that grab it
constexpr double kMinWidth = 1.0;   // cm
const char* kSlotNames[6] = {"Top", "Bottom", "Front", "Back", "Left", "Right"};
} // namespace

Editor::RefImage* Editor::refFor(int view) {
    if (view < 0 || view > 3) return nullptr;
    const int slot = refSlot(views_[view].dir);
    if (slot < 0 || !refs_[slot].tex) return nullptr;
    return &refs_[slot];
}

bool Editor::refLoad(int view, const std::string& path) {
    const int slot = view >= 0 && view <= 3 ? refSlot(views_[view].dir) : -1;
    if (slot < 0) return false;
    int w = 0, h = 0, n = 0;
    unsigned char* px = stbi_load(path.c_str(), &w, &h, &n, 4);
    if (!px) {
        const char* why = stbi_failure_reason();
        showMessage("Could not load the image",
                    fs::path(path).filename().string() + "\n\n" + (why ? why : "unknown error") +
                        "\n\nPNG, JPG, TGA and BMP files can be used.");
        return false;
    }
    const unsigned tex = uploadTexture(px, w, h);
    stbi_image_free(px);
    if (!tex) {
        showMessage("Could not load the image", fs::path(path).filename().string());
        return false;
    }
    RefImage& r = refs_[slot];
    deleteTexture(r.tex);
    r.tex = tex;
    r.name = fs::path(path).filename().string();
    // Fill the visible part of the view (a little margin), proportions kept.
    const View& v = views_[view];
    const double vw = viewImgSize_[view].x > 1 ? viewImgSize_[view].x : 400;
    const double vh = viewImgSize_[view].y > 1 ? viewImgSize_[view].y : 300;
    double u0, v0, u1, v1;
    v.ortho.screenToPlane(vw * 0.05, vh * 0.95, vw, vh, u0, v0);
    v.ortho.screenToPlane(vw * 0.95, vh * 0.05, vw, vh, u1, v1);
    const double alpha = r.place.alpha;
    const bool front = r.place.front;
    r.place = RefPlacement{};
    r.place.alpha = alpha;
    r.place.front = front;
    r.place.fit(u0, v0, u1, v1, (double)h / w);
    refEdit_ = slot;
    markViewsDirty();
    return true;
}

void Editor::refRemove(int slot) {
    if (slot < 0 || slot >= 6) return;
    deleteTexture(refs_[slot].tex);
    refs_[slot] = RefImage{};
    if (refEdit_ == slot) refEdit_ = -1;
    refDragView_ = -1;
    markViewsDirty();
}

void Editor::refChooseFile(int view) {
    const std::string start = path_.empty() ? std::string() : fs::path(path_).parent_path().string();
    if (nativeDialogsAvailable()) {
        std::string path;
        if (openFileDialog(start, {"png", "jpg", "jpeg", "tga", "bmp"}, path)) refLoad(view, path);
        return;
    }
    askPath("Load reference image (PNG, JPG, TGA, BMP)", false, start,
            [this, view](const std::string& p) { refLoad(view, p); });
}

ViewExtras Editor::viewExtras(int view) {
    ViewExtras x;
    if (showFigure_) {
        if (figureMesh_ != doc_.meshVersion()) {
            figureAt_ = figureSpot(doc_.mesh);
            figureMesh_ = doc_.meshVersion();
        }
        x.figure = true;
        x.figureAt = figureAt_;
    }
    if (const RefImage* r = refFor(view)) {
        x.refTex = r->tex;
        x.ref = r->place;
    }
    return x;
}

bool Editor::refInput(int view, ImVec2 imgPos, ImVec2 imgSize) {
    const ImGuiIO& io = ImGui::GetIO();
    View& v = views_[view];
    RefImage* r = refFor(view);
    const bool editing = r && refEdit_ >= 0 && refSlot(v.dir) == refEdit_;
    if (!editing) {
        if (refDragView_ == view) refDragView_ = -1;
        return false;
    }
    const double w = imgSize.x, h = imgSize.y;
    const double mx = io.MousePos.x - imgPos.x, my = io.MousePos.y - imgPos.y;
    double pu, pv;
    v.ortho.screenToPlane(mx, my, w, h, pu, pv);
    RefPlacement& p = r->place;

    // What is under the pointer: a corner (index) or the picture (4).
    auto under = [&]() {
        double U[4], V[4];
        p.corners(U, V);
        for (int k = 0; k < 4; ++k) {
            double sx, sy;
            v.ortho.planeToScreen(U[k], V[k], w, h, sx, sy);
            if (std::fabs(sx - mx) <= kCornerGrab && std::fabs(sy - my) <= kCornerGrab) return k;
        }
        double s, t;
        return p.toPicture(pu, pv, s, t) ? 4 : -1;
    };

    const bool hovered = ImGui::IsItemHovered();
    if (ImGui::IsKeyDown(ImGuiKey_Space) && refDragView_ != view) return false; // Space + drag pans
    if (refDragView_ == view) {
        draggingView_ = view;
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) || !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            refDragView_ = -1;
        } else if (io.MouseDelta.x != 0 || io.MouseDelta.y != 0) {
            // Moves by whole grid steps and sizes to whole steps when snapping is on,
            // like the selection box.
            const double step = grid_.snap ? grid_.size : 0;
            if (refDragCorner_ < 0) {
                double du = pu - refGrabU_, dv = pv - refGrabV_;
                if (step > 0) du = std::round(du / step) * step, dv = std::round(dv / step) * step;
                p.cu = refStartU_ + du;
                p.cv = refStartV_ + dv;
            } else {
                p.dragCorner(refDragCorner_, pu, pv, kMinWidth, step);
            }
            markViewsDirty();
        }
        return true;
    }
    if (!hovered) return false;
    const int at = under();
    if (at >= 0 && at < 4) {
        // Corners 0/2 lie on one diagonal, 1/3 on the other; the cursor follows the
        // picture's turn only roughly (it has no rotated arrows).
        double U[4], V[4], sx[4], sy[4];
        p.corners(U, V);
        for (int k = 0; k < 4; ++k) v.ortho.planeToScreen(U[k], V[k], w, h, sx[k], sy[k]);
        const int o = (at + 2) % 4;
        const bool nwse = (sx[at] - sx[o]) * (sy[at] - sy[o]) > 0;
        ImGui::SetMouseCursor(nwse ? ImGuiMouseCursor_ResizeNWSE : ImGuiMouseCursor_ResizeNESW);
    } else if (at == 4) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    }
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (at == 4) {
            refDragView_ = view;
            refDragCorner_ = -1;
            refGrabU_ = pu;
            refGrabV_ = pv;
            refStartU_ = p.cu;
            refStartV_ = p.cv;
        } else if (at >= 0) {
            refDragView_ = view;
            refDragCorner_ = (at + 2) % 4;
        }
        if (refDragView_ == view) draggingView_ = view;
    }
    // While the image's window is open, the left mouse belongs to the image.
    return ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
           ImGui::IsMouseReleased(ImGuiMouseButton_Left);
}

void Editor::refOverlay(int view, ImVec2 imgPos, ImVec2 imgSize, ImDrawList* dl) {
    const RefImage* r = refFor(view);
    if (!r) return;
    const View& v = views_[view];
    double U[4], V[4];
    r->place.corners(U, V);
    ImVec2 s[4];
    for (int k = 0; k < 4; ++k) {
        double x, y;
        v.ortho.planeToScreen(U[k], V[k], imgSize.x, imgSize.y, x, y);
        s[k] = {imgPos.x + (float)x, imgPos.y + (float)y};
    }
    const ImU32 col = ImGui::GetColorU32(theme::kAccent);
    dl->AddPolyline(s, 4, col, ImDrawFlags_Closed, 1.5f);
    const float hs = 4.0f;
    for (const ImVec2& c : s) {
        dl->AddRectFilled({c.x - hs, c.y - hs}, {c.x + hs, c.y + hs}, IM_COL32(255, 255, 255, 255));
        dl->AddRect({c.x - hs, c.y - hs}, {c.x + hs, c.y + hs}, col, 0, 0, 1.5f);
    }
}

void Editor::refWindow() {
    if (refEdit_ < 0) return;
    RefImage& r = refs_[refEdit_];
    if (!r.tex) {
        refEdit_ = -1;
        return;
    }
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({vp->WorkPos.x + vp->WorkSize.x * 0.615f, vp->WorkPos.y + vp->WorkSize.y * 0.06f},
                            ImGuiCond_FirstUseEver);
    bool open = true;
    char title[64];
    std::snprintf(title, sizeof title, "Reference image (%s)###refimage", kSlotNames[refEdit_]);
    if (!ImGui::Begin(title, &open, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking)) {
        ImGui::End();
        if (!open) {
            refEdit_ = -1;
            markViewsDirty();
        }
        return;
    }
    const int slot = refEdit_;
    RefPlacement& p = r.place;
    bool changed = false;
    ImGui::TextUnformatted(r.name.c_str());
    ImGui::TextDisabled("Drag it to move it, a corner to resize it.");
    const float fieldW = ImGui::GetFontSize() * 8;
    ImGui::SetNextItemWidth(fieldW);
    if (ImGui::InputDouble("Width (cm)", &p.width, 0, 0, "%.4g")) {
        p.width = std::max(kMinWidth, p.width);
        changed = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("height %.4g cm", p.height());
    ImGui::SetNextItemWidth(fieldW);
    changed |= ImGui::InputDouble("Rotation (degrees)", &p.rot, 0, 0, "%.4g");
    int transparency = (int)std::lround((1.0 - p.alpha) * 100);
    ImGui::SetNextItemWidth(fieldW);
    if (ImGui::SliderInt("Transparency (%)", &transparency, 0, 95)) {
        p.alpha = 1.0 - transparency / 100.0;
        changed = true;
    }
    if (ImGui::RadioButton("Behind the model", !p.front)) {
        p.front = false;
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("In front", p.front)) {
        p.front = true;
        changed = true;
    }
    ImGui::Separator();
    int viewHere = -1;
    for (int i = 0; i < 4 && viewHere < 0; ++i)
        if (refSlot(views_[i].dir) == slot) viewHere = i;
    if (ImGui::Button("Load another...") && viewHere >= 0) refChooseFile(viewHere);
    ImGui::SameLine();
    const bool remove = ImGui::Button("Remove");
    ImGui::SameLine();
    const bool done = ImGui::Button("Done");
    ImGui::End();
    if (changed) markViewsDirty();
    if (remove) refRemove(slot);
    else if (done || !open) {
        refEdit_ = -1;
        markViewsDirty();
    }
}

} // namespace mazz
