#pragma once
// The open model and its undo history. Every change goes through `change()`, which keeps
// a copy of the model and selection before and after (models are small, so whole copies
// are simple and safe).
#include "core/camera.hpp"
#include "core/mesh.hpp"
#include "core/ops.hpp"
#include <functional>
#include <string>
#include <vector>

namespace mazz {

// A picture pinned to one view direction (e.g. a photo of the front of the object).
struct RefImage {
    ViewDir dir = ViewDir::Front;
    std::string path;        // as written in the file (relative to the model file if possible)
    double x = 0, y = 0;     // centre, in the view's plane (cm)
    double rotation = 0;     // degrees, counter-clockwise
    double width = 100;      // cm (height follows the image's proportions)
    double alpha = 0.5;      // 0..1
    bool inFront = false;    // drawn over the model instead of behind it
    bool operator==(const RefImage& o) const;
};

struct Model {
    Mesh mesh;
    Vec3 pivot{0, 0, 0};     // the model's origin (placement point in Scalpellino)
    double texel = 0.5;      // pixels per cm target (milestone 5)
    std::vector<RefImage> refs;
};

struct Snapshot {
    Model model;
    Selection sel;
};

class Document {
public:
    Model model;
    Selection sel;
    std::string path;        // empty = never saved
    bool dirty = false;

    // Applies a change as one undo step named `what`. The function edits model and
    // selection; if it reports no change (returns false), nothing is recorded.
    bool change(const std::string& what, const std::function<bool(Model&, Selection&)>& fn);
    // Selection-only changes are not undo steps.
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    std::string undoName() const { return undo_.empty() ? "" : undo_.back().name; }
    std::string redoName() const { return redo_.empty() ? "" : redo_.back().name; }
    bool undo();
    bool redo();
    void clearHistory() { undo_.clear(); redo_.clear(); }
    // Marks the current state as saved.
    void markSaved() { dirty = false; savedSerial_ = serial_; }
    void reset();             // empty model, no history

private:
    struct Step {
        std::string name;
        Snapshot before, after;
        long serialBefore, serialAfter;
    };
    std::vector<Step> undo_, redo_;
    long serial_ = 0, nextSerial_ = 1, savedSerial_ = 0;
    static constexpr size_t kMaxSteps = 500;
};

} // namespace mazz
