#pragma once
// The open model and its undo history (ARCHITECTURE §10).
//
// Every change goes through a MeshEdit, which applies it to the mesh right away and
// remembers the old value of each point and face the first time it is touched. A
// finished edit becomes a MeshChange holding only those before/after values (never a
// copy of the whole mesh). Each undo step also stores the selection, mode included,
// before and after.
//
// Two ways to change the document:
//  - apply(name, fn): a one-shot command (a delete, a typed transform, a new box).
//  - beginLive() ... commitLive(name) / cancelLive(): an edit that stays open across
//    frames (a drag): the tool keeps setting positions, the views show them live, and
//    it becomes one undo step on commit, or is rolled back on cancel.
#include "core/mesh.hpp"
#include "core/selection.hpp"
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace mazz {

struct MeshChange {
    int oldVerts = 0, newVerts = 0, oldFaces = 0, newFaces = 0;
    std::vector<std::pair<int, Vec3>> vertBefore, vertAfter;
    std::vector<std::pair<int, Face>> faceBefore, faceAfter;

    bool empty() const;
    std::size_t bytes() const;
    void undo(Mesh& m) const;
    void redo(Mesh& m) const;
};

class MeshEdit {
public:
    explicit MeshEdit(Mesh& m);
    const Mesh& mesh() const { return m_; }

    int addVert(const Vec3& p);              // cleaned
    void setVert(int i, const Vec3& p);      // cleaned
    int addFace(const Face& f);
    void setFace(int i, const Face& f);
    void resizeVerts(int n);
    void resizeFaces(int n);
    // Removes faces, then points no face uses any more. Fills `faceMap` (old face index
    // -> new index, -1 if removed) if given.
    void deleteFaces(const std::set<int>& faces, std::vector<int>* faceMap = nullptr);

    // The change so far (only values that really differ are kept).
    MeshChange change() const;
    // Puts back everything as it was before the edit.
    void revert();

private:
    Mesh& m_;
    int oldVerts_, oldFaces_;
    std::map<int, Vec3> vertBefore_;
    std::map<int, Face> faceBefore_;
    void touchVert(int i);
    void touchFace(int i);
};

class Document {
public:
    Document() = default;
    Document(const Document&) = delete; // a live edit points into `mesh`
    Document& operator=(const Document&) = delete;

    Mesh mesh;
    Selection sel;

    // A one-shot command. `fn` edits through the MeshEdit and may change the selection;
    // it returns false to cancel (nothing is kept). Returns true if a step was recorded.
    bool apply(const std::string& name, const std::function<bool(MeshEdit&, Selection&)>& fn);

    // A live edit (see top). Only one at a time.
    void beginLive();
    bool liveActive() const { return live_ != nullptr; }
    MeshEdit& live() { return *live_; }
    bool commitLive(const std::string& name); // false if nothing changed (no step)
    void cancelLive();

    // Selection-only changes are not undo steps.
    void select(const Selection& s) { sel = s; }

    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    std::string undoName() const { return canUndo() ? undo_.back().name : std::string(); }
    std::string redoName() const { return canRedo() ? redo_.back().name : std::string(); }
    bool undo();
    bool redo();
    int undoCount() const { return (int)undo_.size(); }

    // Saved / unsaved, as of the last save or open; undo and redo respect it.
    bool dirty() const { return serial_ != savedSerial_; }
    void markSaved() { savedSerial_ = serial_; }
    // A new or opened model: empty history, not dirty.
    void reset(Mesh m = Mesh());

    // Memory the history may use; the oldest steps go first.
    static constexpr std::size_t kHistoryBytes = 64u * 1024 * 1024;
    std::size_t historyBytes() const { return historyBytes_; }

private:
    struct Step {
        std::string name;
        MeshChange change;
        Selection selBefore, selAfter;
        long serial = 0, prevSerial = 0;
        std::size_t bytes = 0;
    };
    std::vector<Step> undo_, redo_;
    std::size_t historyBytes_ = 0;
    long serial_ = 0, savedSerial_ = 0, nextSerial_ = 1;

    std::unique_ptr<MeshEdit> live_;
    Selection liveSelBefore_;

    bool record(const std::string& name, MeshChange c, const Selection& before);
    void trim();
};

} // namespace mazz
