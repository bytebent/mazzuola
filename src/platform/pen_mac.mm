// macOS pen input: see pen.hpp.
#import <AppKit/AppKit.h>
#include "platform/pen.hpp"
#include "imgui.h"
#include <cmath>

namespace mazz {

namespace {
PenState gPen;
id gMonitor = nil;

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

// The event's position in ImGui coordinates, if it belongs to one of our windows.
bool imguiPosition(NSEvent* e, ImVec2& out) {
    NSWindow* w = e.window;
    if (!w || !ImGui::GetCurrentContext()) return false;
    ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
    for (ImGuiViewport* vp : pio.Viewports) {
        if (vp->PlatformHandleRaw != (__bridge void*)w) continue;
        NSView* view = w.contentView;
        const NSPoint p = [view convertPoint:e.locationInWindow fromView:nil];
        const double yTop = view.isFlipped ? p.y : view.bounds.size.height - p.y;
        // With viewports enabled ImGui works in desktop coordinates; otherwise in the
        // main window's own coordinates (its Pos is then 0,0).
        const bool desktop = (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0;
        const ImVec2 base = desktop ? vp->Pos : ImVec2(0, 0);
        out = ImVec2(base.x + (float)p.x, base.y + (float)yTop);
        return true;
    }
    return false;
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
        if (!ImGui::GetCurrentContext()) return e;
        ImGuiIO& io = ImGui::GetIO();
        io.AddMouseSourceEvent(pen ? ImGuiMouseSource_Pen : ImGuiMouseSource_Mouse);
        // Before a click, make sure ImGui knows where it is (GLFW doesn't tell it).
        const bool press = e.type == NSEventTypeLeftMouseDown || e.type == NSEventTypeRightMouseDown ||
                           e.type == NSEventTypeOtherMouseDown || e.type == NSEventTypeLeftMouseUp;
        ImVec2 p;
        if (press && imguiPosition(e, p)) {
            if (std::fabs(p.x - io.MousePos.x) > 0.5f || std::fabs(p.y - io.MousePos.y) > 0.5f) ++gPen.positionFixes;
            io.AddMousePosEvent(p.x, p.y);
            gPen.x = p.x;
            gPen.y = p.y;
        }
        return e;
    }];
}

const PenState& penState() { return gPen; }

} // namespace mazz
