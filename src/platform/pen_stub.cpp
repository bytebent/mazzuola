// Non-macOS builds: no pen input handling (see pen.hpp).
#include "platform/pen.hpp"

namespace mazz {
namespace {
PenState gPen;
}
void installPenInput() {}
void registerPenContext(ImGuiContext*) {}
void unregisterPenContext(ImGuiContext*) {}
const PenState& penState() { return gPen; }
} // namespace mazz
