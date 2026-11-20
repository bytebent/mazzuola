// The four model views (or one, maximised): splitters, per-view header (direction,
// 3D camera mode, maximise), the view image, and mouse navigation.
#include "app/editor.hpp"
#include "ui/theme.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace mazz {

namespace {
constexpr float kSplitter = 6.0f; // logical px between views
constexpr float kMinCell = 80.0f;

ImTextureID texId(unsigned tex) { return (ImTextureID)(intptr_t)tex; }
} // namespace

void Editor::viewsPanel() {
    hoveredView_ = -1;
    haveCursorWorld_ = false;
    draggingView_ = -1; // set again below while a view or splitter is being dragged
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 10 || avail.y < 10) return;

    if (layout_.maximised >= 0) {
        viewCell(layout_.maximised, origin, avail);
        return;
    }

    // Splitters: a vertical bar between columns and a horizontal bar between rows.
    const float leftW = std::clamp(std::round((avail.x - kSplitter) * layout_.splitX), kMinCell, avail.x - kSplitter - kMinCell);
    const float topH = std::clamp(std::round((avail.y - kSplitter) * layout_.splitY), kMinCell, avail.y - kSplitter - kMinCell);
    const float rightW = avail.x - kSplitter - leftW, bottomH = avail.y - kSplitter - topH;

    viewCell(0, origin, {leftW, topH});
    viewCell(1, {origin.x + leftW + kSplitter, origin.y}, {rightW, topH});
    viewCell(2, {origin.x, origin.y + topH + kSplitter}, {leftW, bottomH});
    viewCell(3, {origin.x + leftW + kSplitter, origin.y + topH + kSplitter}, {rightW, bottomH});

    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto splitter = [&](const char* id, ImVec2 pos, ImVec2 size, bool vertical) {
        ImGui::SetCursorScreenPos(pos);
        ImGui::InvisibleButton(id, size);
        const bool hot = ImGui::IsItemHovered() || ImGui::IsItemActive();
        if (hot) ImGui::SetMouseCursor(vertical ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
        if (ImGui::IsItemActive()) {
            const ImVec2 d = ImGui::GetIO().MouseDelta;
            if (vertical) layout_.splitX = std::clamp(layout_.splitX + d.x / (avail.x - kSplitter), 0.1f, 0.9f);
            else layout_.splitY = std::clamp(layout_.splitY + d.y / (avail.y - kSplitter), 0.1f, 0.9f);
            draggingView_ = 99; // keep frames coming while dragging
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            if (vertical) layout_.splitX = 0.5f;
            else layout_.splitY = 0.5f;
        }
        if (hot) dl->AddRectFilled(pos, {pos.x + size.x, pos.y + size.y}, ImGui::GetColorU32(theme::kAccentSoft));
    };
    splitter("##splitV", {origin.x + leftW, origin.y}, {kSplitter, avail.y}, true);
    splitter("##splitH", {origin.x, origin.y + topH}, {avail.x, kSplitter}, false);
}

void Editor::viewCell(int i, ImVec2 pos, ImVec2 size) {
    View& v = views_[i];
    ImGui::PushID(i);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float headerH = ImGui::GetFrameHeight() + 4;

    // ---- header: direction, 3D camera mode, maximise
    dl->AddRectFilled(pos, {pos.x + size.x, pos.y + headerH}, ImGui::GetColorU32(ImGuiCol_MenuBarBg));
    ImGui::SetCursorScreenPos({pos.x + 4, pos.y + 2});
    ImGui::SetNextItemWidth(ImGui::CalcTextSize("Bottom").x + ImGui::GetFrameHeight() + 16);
    if (ImGui::BeginCombo("##dir", viewDirName(v.dir))) {
        for (int d = 0; d < kViewDirCount; ++d) {
            const ViewDir vd = (ViewDir)d;
            if (ImGui::Selectable(viewDirName(vd), v.dir == vd)) {
                if (flying_ && flyView_ == i) setFlying(false, -1);
                v.setDir(vd);
            }
        }
        ImGui::EndCombo();
    }
    if (v.is3D()) {
        ImGui::SameLine();
        auto toggle = [&](const char* label, bool on, Act a, const char* tip) {
            if (on) {
                ImGui::PushStyleColor(ImGuiCol_Button, theme::kAccentSoft);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::kAccentSoftHover);
            }
            if (ImGui::SmallButton(label)) {
                if (a == Act::FlyMode) {
                    if (flying_) setFlying(false, -1);
                    else setFlying(true, i);
                } else {
                    run(a);
                }
            }
            if (on) ImGui::PopStyleColor(2);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
        };
        toggle("Origin", !orbitSelection_, Act::OrbitOrigin, "Orbit around the origin (right mouse drag)");
        ImGui::SameLine();
        toggle("Selection", orbitSelection_, Act::OrbitSelection, "Orbit around the selection (the origin while nothing is selected)");
        ImGui::SameLine();
        toggle("Fly", flying_ && flyView_ == i, Act::FlyMode, "Fly mode: WASD, Q/E down/up, Shift fast; Z or Esc stops");
    }
    {
        // Right-aligned on the same line as the direction combo.
        const char* label = layout_.maximised == i ? "Restore" : "Maximise";
        const float bw = ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2;
        ImGui::SameLine();
        const float x = pos.x + size.x - bw - 6 - ImGui::GetWindowPos().x;
        if (x > ImGui::GetCursorPosX()) ImGui::SetCursorPosX(x);
        if (ImGui::SmallButton(label)) {
            layout_.maximised = layout_.maximised == i ? -1 : i;
            markViewsDirty();
        }
    }

    // ---- the view image
    const ImVec2 imgPos{pos.x, pos.y + headerH};
    const ImVec2 imgSize{size.x, std::max(1.0f, size.y - headerH)};
    viewImgPos_[i] = imgPos;
    viewImgSize_[i] = imgSize;
    drawer_.draw(v, grid_, imgSize.x, imgSize.y, viewScale());
    ImGui::SetCursorScreenPos(imgPos);
    ImGui::InvisibleButton("##view", imgSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    dl->AddImage(texId(v.target.texture()), imgPos, {imgPos.x + imgSize.x, imgPos.y + imgSize.y}, {0, 1}, {1, 0});
    viewInput(i, imgPos, imgSize);

    // The active tool's handles and previews, over the image (a light layer: the
    // model's drawing isn't rebuilt for them).
    dl->PushClipRect(imgPos, {imgPos.x + imgSize.x, imgPos.y + imgSize.y}, true);
    tool_->overlay(toolEnv(), geom(i), i, dl);
    dl->PopClipRect();

    // An accent frame around the view the fly mode drives.
    if (flying_ && flyView_ == i)
        dl->AddRect(imgPos, {imgPos.x + imgSize.x, imgPos.y + imgSize.y}, ImGui::GetColorU32(theme::kAccent), 0.0f, 0, 2.0f);
    ImGui::PopID();
}

void Editor::viewInput(int i, ImVec2 imgPos, ImVec2 imgSize) {
    View& v = views_[i];
    const ImGuiIO& io = ImGui::GetIO();
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    if (hovered) hoveredView_ = i;
    const double mx = io.MousePos.x - imgPos.x, my = io.MousePos.y - imgPos.y;
    const double w = imgSize.x, h = imgSize.y;

    if (flying_ && flyView_ == i) return; // the fly camera owns the mouse

    // Left mouse goes to the active tool (Space + left pans instead); not while an
    // operation window is open (its preview is a live edit).
    if (op_ == Op::None) {
        const bool space = ImGui::IsKeyDown(ImGuiKey_Space);
        ToolEnv env = toolEnv();
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !space && toolView_ < 0) {
            toolView_ = i;
            tool_->press(env, pointer(i));
        } else if (toolView_ == i && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            toolView_ = -1;
            tool_->release(env, pointer(i));
        } else if (toolView_ == i && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
                   (io.MouseDelta.x != 0 || io.MouseDelta.y != 0)) {
            tool_->drag(env, pointer(i));
        } else if (hovered && toolView_ < 0 && !ImGui::IsAnyMouseDown() &&
                   (io.MouseDelta.x != 0 || io.MouseDelta.y != 0)) {
            tool_->hover(env, pointer(i));
        }
        if (toolView_ == i) draggingView_ = i;
    }

    // Cursor position readout (2D views).
    if (hovered && !v.is3D()) {
        double u, vv;
        v.ortho.screenToPlane(mx, my, w, h, u, vv);
        cursorWorld_ = v.ortho.planeToWorld(u, vv);
        cursorView_ = i;
        haveCursorWorld_ = true;
    }

    // Wheel: zoom (toward the cursor in 2D).
    if (hovered && io.MouseWheel != 0) {
        if (v.is3D()) v.cam3d.dolly(std::pow(0.87, (double)io.MouseWheel));
        else v.ortho.zoomAt(std::pow(1.15, (double)io.MouseWheel), mx, my, w, h);
        v.dirty = true;
    }

    if (!active) return;
    draggingView_ = i;
    const ImVec2 d = io.MouseDelta;
    const bool panning = ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
                         (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsKeyDown(ImGuiKey_Space));
    const bool orbiting = v.is3D() && ImGui::IsMouseDown(ImGuiMouseButton_Right);
    if (d.x == 0 && d.y == 0) return;
    if (panning) {
        if (v.is3D()) v.cam3d.panPx(d.x, d.y, h);
        else v.ortho.pan(d.x, d.y);
        v.dirty = true;
    } else if (orbiting) {
        v.cam3d.orbit(-d.x * 0.35, d.y * 0.35);
        v.dirty = true;
    }
}

} // namespace mazz
