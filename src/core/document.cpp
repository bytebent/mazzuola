#include "core/document.hpp"
#include "core/numbers.hpp"
#include <algorithm>

namespace mazz {

// ---------------------------------------------------------------- MeshChange

bool MeshChange::empty() const {
    return oldVerts == newVerts && oldFaces == newFaces && vertBefore.empty() && vertAfter.empty() &&
           faceBefore.empty() && faceAfter.empty();
}

std::size_t MeshChange::bytes() const {
    return sizeof(*this) + (vertBefore.size() + vertAfter.size()) * sizeof(std::pair<int, Vec3>) +
           (faceBefore.size() + faceAfter.size()) * sizeof(std::pair<int, Face>);
}

void MeshChange::undo(Mesh& m) const {
    m.verts.resize(oldVerts);
    m.faces.resize(oldFaces);
    for (const auto& [i, p] : vertBefore) m.verts[i] = p;
    for (const auto& [i, f] : faceBefore) m.faces[i] = f;
}

void MeshChange::redo(Mesh& m) const {
    m.verts.resize(newVerts);
    m.faces.resize(newFaces);
    for (const auto& [i, p] : vertAfter) m.verts[i] = p;
    for (const auto& [i, f] : faceAfter) m.faces[i] = f;
}

// ---------------------------------------------------------------- MeshEdit

MeshEdit::MeshEdit(Mesh& m, long* version) : m_(m), version_(version), oldVerts_((int)m.verts.size()), oldFaces_((int)m.faces.size()) {}

void MeshEdit::touchVert(int i) {
    if (i < oldVerts_ && !vertBefore_.count(i)) vertBefore_[i] = m_.verts[i];
}
void MeshEdit::touchFace(int i) {
    if (i < oldFaces_ && !faceBefore_.count(i)) faceBefore_[i] = m_.faces[i];
}

int MeshEdit::addVert(const Vec3& p) {
    m_.verts.push_back(clean(p));
    bump();
    return (int)m_.verts.size() - 1;
}

void MeshEdit::setVert(int i, const Vec3& p) {
    touchVert(i);
    m_.verts[i] = clean(p);
    bump();
}

int MeshEdit::addFace(const Face& f) {
    m_.faces.push_back(f);
    bump();
    return (int)m_.faces.size() - 1;
}

void MeshEdit::setFace(int i, const Face& f) {
    touchFace(i);
    m_.faces[i] = f;
    bump();
}

void MeshEdit::resizeVerts(int n) {
    for (int i = n; i < (int)m_.verts.size(); ++i) touchVert(i);
    m_.verts.resize(n);
    bump();
}

void MeshEdit::resizeFaces(int n) {
    for (int i = n; i < (int)m_.faces.size(); ++i) touchFace(i);
    m_.faces.resize(n);
    bump();
}

void MeshEdit::deleteFaces(const std::set<int>& faces, std::vector<int>* faceMap) {
    const int nf = (int)m_.faces.size();
    std::vector<int> fmap(nf, -1);
    int out = 0;
    for (int i = 0; i < nf; ++i) {
        if (faces.count(i)) continue;
        fmap[i] = out;
        if (out != i) setFace(out, m_.faces[i]);
        ++out;
    }
    resizeFaces(out);
    // Points no face uses any more.
    const int nv = (int)m_.verts.size();
    std::vector<char> used(nv, 0);
    for (const Face& f : m_.faces)
        for (int k = 0; k < f.n; ++k) used[f.v[k]] = 1;
    std::vector<int> vmap(nv, -1);
    int vo = 0;
    for (int i = 0; i < nv; ++i) {
        if (!used[i]) continue;
        vmap[i] = vo;
        if (vo != i) setVert(vo, m_.verts[i]);
        ++vo;
    }
    if (vo != nv) {
        resizeVerts(vo);
        for (int i = 0; i < (int)m_.faces.size(); ++i) {
            Face f = m_.faces[i];
            bool moved = false;
            for (int k = 0; k < f.n; ++k)
                if (vmap[f.v[k]] != f.v[k]) {
                    f.v[k] = vmap[f.v[k]];
                    moved = true;
                }
            if (moved) setFace(i, f);
        }
    }
    if (faceMap) *faceMap = fmap;
}

MeshChange MeshEdit::change() const {
    MeshChange c;
    c.oldVerts = oldVerts_;
    c.oldFaces = oldFaces_;
    c.newVerts = (int)m_.verts.size();
    c.newFaces = (int)m_.faces.size();
    for (const auto& [i, before] : vertBefore_) {
        const bool kept = i < c.newVerts;
        if (kept && m_.verts[i].x == before.x && m_.verts[i].y == before.y && m_.verts[i].z == before.z) continue;
        c.vertBefore.emplace_back(i, before);
        if (kept) c.vertAfter.emplace_back(i, m_.verts[i]);
    }
    for (int i = oldVerts_; i < c.newVerts; ++i) c.vertAfter.emplace_back(i, m_.verts[i]);
    for (const auto& [i, before] : faceBefore_) {
        const bool kept = i < c.newFaces;
        if (kept && m_.faces[i] == before) continue;
        c.faceBefore.emplace_back(i, before);
        if (kept) c.faceAfter.emplace_back(i, m_.faces[i]);
    }
    for (int i = oldFaces_; i < c.newFaces; ++i) c.faceAfter.emplace_back(i, m_.faces[i]);
    return c;
}

void MeshEdit::revert() {
    m_.verts.resize(oldVerts_);
    m_.faces.resize(oldFaces_);
    for (const auto& [i, p] : vertBefore_) m_.verts[i] = p;
    for (const auto& [i, f] : faceBefore_) m_.faces[i] = f;
    vertBefore_.clear();
    faceBefore_.clear();
    bump();
}

// ---------------------------------------------------------------- Document

bool Document::apply(const std::string& name, const std::function<bool(MeshEdit&, Selection&)>& fn) {
    cancelLive();
    const Selection before = sel;
    MeshEdit e(mesh, &meshVersion_);
    Selection s = sel;
    if (!fn(e, s)) {
        e.revert();
        return false;
    }
    if (s != sel) ++selVersion_;
    sel = s;
    dropMissing(mesh, sel);
    return record(name, e.change(), before);
}

void Document::beginLive() {
    cancelLive();
    live_ = std::make_unique<MeshEdit>(mesh, &meshVersion_);
    liveSelBefore_ = sel;
}

bool Document::commitLive(const std::string& name) {
    if (!live_) return false;
    const MeshChange c = live_->change();
    live_.reset();
    dropMissing(mesh, sel);
    return record(name, c, liveSelBefore_);
}

void Document::cancelLive() {
    if (!live_) return;
    live_->revert();
    live_.reset();
    sel = liveSelBefore_;
    ++selVersion_;
}

bool Document::record(const std::string& name, MeshChange c, const Selection& before) {
    if (c.empty()) return false; // a selection change alone is not a step
    Step st;
    st.name = name;
    st.change = std::move(c);
    st.selBefore = before;
    st.selAfter = sel;
    st.prevSerial = serial_;
    st.serial = nextSerial_++;
    st.bytes = st.change.bytes() + sizeof(Step);
    serial_ = st.serial;
    for (const Step& r : redo_) historyBytes_ -= r.bytes;
    redo_.clear();
    historyBytes_ += st.bytes;
    undo_.push_back(std::move(st));
    trim();
    return true;
}

void Document::trim() {
    std::size_t drop = 0;
    while (drop + 1 < undo_.size() && historyBytes_ > kHistoryBytes) historyBytes_ -= undo_[drop++].bytes;
    if (drop) undo_.erase(undo_.begin(), undo_.begin() + (long)drop);
}

bool Document::undo() {
    cancelLive();
    if (undo_.empty()) return false;
    Step st = std::move(undo_.back());
    undo_.pop_back();
    st.change.undo(mesh);
    ++meshVersion_;
    ++selVersion_;
    sel = st.selBefore;
    dropMissing(mesh, sel);
    serial_ = st.prevSerial;
    redo_.push_back(std::move(st));
    return true;
}

bool Document::redo() {
    cancelLive();
    if (redo_.empty()) return false;
    Step st = std::move(redo_.back());
    redo_.pop_back();
    st.change.redo(mesh);
    ++meshVersion_;
    ++selVersion_;
    sel = st.selAfter;
    dropMissing(mesh, sel);
    serial_ = st.serial;
    undo_.push_back(std::move(st));
    return true;
}

void Document::reset(Mesh m) {
    cancelLive();
    mesh = std::move(m);
    sel = Selection();
    ++meshVersion_;
    ++selVersion_;
    undo_.clear();
    redo_.clear();
    historyBytes_ = 0;
    serial_ = savedSerial_ = 0;
}

} // namespace mazz
