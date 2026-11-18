// macOS pen input: see pen.hpp.
#import <AppKit/AppKit.h>
#include "platform/pen.hpp"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace mazz {

namespace {
PenState gPen;
id gMonitor = nil;
std::vector<ImGuiContext*> gContexts;

bool isPenEvent(NSEvent* e) {
    switch (e.type) {
    case NSEventTypeTabletPoint:
    case NSEventTypeTabletProximity: return true;
    case NSEventTypeLeftMouseDown:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeRightMouseDown:
    case NSEventTypeRightMouseUp:
    case NSEventTypeOtherMouseDown:
    case NSEventTypeOtherMouseUp:
    case NSEventTypeMouseMoved:
        return e.subtype == NSEventSubtypeTabletPoint || e.subtype == NSEventSubtypeTabletProximity;
    default: return false;
    }
}

const char* eventName(NSEvent* e) {
    switch (e.type) {
    case NSEventTypeLeftMouseDown: return "left down";
    case NSEventTypeLeftMouseUp: return "left up";
    case NSEventTypeLeftMouseDragged: return "left drag";
    case NSEventTypeRightMouseDown: return "right down";
    case NSEventTypeRightMouseUp: return "right up";
    case NSEventTypeOtherMouseDown: return "middle down";
    case NSEventTypeOtherMouseUp: return "middle up";
    case NSEventTypeMouseMoved: return "move";
    case NSEventTypeTabletPoint: return "tablet point";
    case NSEventTypeTabletProximity: return "tablet proximity";
    default: return "other";
    }
}

// Finds which of our ImGui contexts owns the event's window, and the event's position in
// that context's coordinates. Leaves that context current (caller restores).
ImGuiContext* findContext(NSEvent* e, ImVec2& out) {
    NSWindow* w = e.window;
    if (!w) return nullptr;
    for (ImGuiContext* ctx : gContexts) {
        ImGui::SetCurrentContext(ctx);
        ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
        for (ImGuiViewport* vp : pio.Viewports) {
            if (vp->PlatformHandleRaw != (__bridge void*)w) continue;
            NSView* view = w.contentView;
            const NSPoint p = [view convertPoint:e.locationInWindow fromView:nil];
            const double yTop = view.isFlipped ? p.y : view.bounds.size.height - p.y;
            // With viewports enabled ImGui works in desktop coordinates; otherwise in the
            // window's own coordinates.
            const bool desktop = (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0;
            const ImVec2 base = desktop ? vp->Pos : ImVec2(0, 0);
            out = ImVec2(base.x + (float)p.x, base.y + (float)yTop);
            return ctx;
        }
    }
    return nullptr;
}
} // namespace

void installPenInput() {
    if (gMonitor) return;
    const NSEventMask mask = NSEventMaskLeftMouseDown | NSEventMaskLeftMouseUp | NSEventMaskLeftMouseDragged |
                             NSEventMaskRightMouseDown | NSEventMaskRightMouseUp | NSEventMaskOtherMouseDown |
                             NSEventMaskOtherMouseUp | NSEventMaskMouseMoved | NSEventMaskTabletPoint |
                             NSEventMaskTabletProximity;
    gMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:mask handler:^NSEvent*(NSEvent* e) {
        ++gPen.events;
        gPen.lastEvent = eventName(e);
        const bool pen = isPenEvent(e);
        gPen.pen = pen;
        if (pen && e.type != NSEventTypeTabletProximity) {
            gPen.pressure = e.pressure;
            gPen.tiltX = (float)e.tilt.x;
            gPen.tiltY = (float)e.tilt.y;
        }
        ImGuiContext* prev = ImGui::GetCurrentContext();
        ImVec2 p;
        if (ImGuiContext* ctx = findContext(e, p)) {
            ImGuiIO& io = ImGui::GetIO(); // ctx is current now
            io.AddMouseSourceEvent(pen ? ImGuiMouseSource_Pen : ImGuiMouseSource_Mouse);
            // Before a click, make sure ImGui knows where it is (GLFW doesn't tell it).
            const bool press = e.type == NSEventTypeLeftMouseDown || e.type == NSEventTypeRightMouseDown ||
                               e.type == NSEventTypeOtherMouseDown || e.type == NSEventTypeLeftMouseUp;
            if (press) {
                if (std::fabs(p.x - io.MousePos.x) > 0.5f || std::fabs(p.y - io.MousePos.y) > 0.5f) ++gPen.positionFixes;
                io.AddMousePosEvent(p.x, p.y);
                gPen.x = p.x;
                gPen.y = p.y;
            }
            (void)ctx;
        }
        ImGui::SetCurrentContext(prev);
        return e;
    }];
}

const PenState& penState() { return gPen; }

void registerPenContext(ImGuiContext* ctx) {
    if (std::find(gContexts.begin(), gContexts.end(), ctx) == gContexts.end()) gContexts.push_back(ctx);
}

void unregisterPenContext(ImGuiContext* ctx) {
    gContexts.erase(std::remove(gContexts.begin(), gContexts.end(), ctx), gContexts.end());
}

} // namespace mazz
