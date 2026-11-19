#include "core/document.hpp"

namespace mazz {

bool RefImage::operator==(const RefImage& o) const {
    return dir == o.dir && path == o.path && x == o.x && y == o.y && rotation == o.rotation && width == o.width &&
           alpha == o.alpha && inFront == o.inFront;
}

bool Document::change(const std::string& what, const std::function<bool(Model&, Selection&)>& fn) {
    Step s;
    s.name = what;
    s.before = {model, sel};
    if (!fn(model, sel)) return false;
    cleanSelection(model.mesh, sel);
    s.after = {model, sel};
    s.serialBefore = serial_;
    serial_ = nextSerial_++;
    s.serialAfter = serial_;
    undo_.push_back(std::move(s));
    if (undo_.size() > kMaxSteps) undo_.erase(undo_.begin());
    redo_.clear();
    dirty = serial_ != savedSerial_;
    return true;
}

bool Document::commitFrom(const Snapshot& before, const std::string& what) {
    Snapshot now{model, sel};
    model = before.model;
    sel = before.sel;
    return change(what, [&](Model& m, Selection& s) {
        m = now.model;
        s = now.sel;
        return true;
    });
}

bool Document::undo() {
    if (undo_.empty()) return false;
    Step s = std::move(undo_.back());
    undo_.pop_back();
    model = s.before.model;
    sel = s.before.sel;
    serial_ = s.serialBefore;
    redo_.push_back(std::move(s));
    dirty = serial_ != savedSerial_;
    return true;
}

bool Document::redo() {
    if (redo_.empty()) return false;
    Step s = std::move(redo_.back());
    redo_.pop_back();
    model = s.after.model;
    sel = s.after.sel;
    serial_ = s.serialAfter;
    undo_.push_back(std::move(s));
    dirty = serial_ != savedSerial_;
    return true;
}

void Document::reset() {
    model = Model{};
    sel.clear();
    path.clear();
    undo_.clear();
    redo_.clear();
    serial_ = 0;
    nextSerial_ = 1;
    savedSerial_ = 0;
    dirty = false;
}

} // namespace mazz
