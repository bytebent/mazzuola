#pragma once
// Typed operations on the selection (ARCHITECTURE §6 Transform, Multi-Replicate,
// Duplicate, Mirror, Flatten). They work on the points the selection covers (any mode)
// through a MeshEdit, so they are undoable and their results clean; faces that end up
// mirrored are turned round so they still face outward.
#include "core/document.hpp"
#include <set>

namespace mazz {

enum class PivotKind { Centre, Origin, Typed };

struct TransformParams {
    Vec3 move;                  // cm
    Vec3 rotate;                // degrees about X, then Y, then Z
    Vec3 scale{100, 100, 100};  // %
    double alongNormal = 0;     // cm along the selection's normal
    PivotKind pivot = PivotKind::Centre;
    Vec3 typed;                 // the typed pivot
};

struct ReplicateParams {
    int copies = 3;
    Vec3 offset{32, 0, 0};      // cm per copy
    bool scaleOffset = false;   // the offset grows / shrinks with the copies
    Vec3 turn;                  // degrees per copy about X, then Y, then Z
    Vec3 scale{100, 100, 100};  // % per copy
    PivotKind pivot = PivotKind::Centre; // Centre = travelling with the copies
    Vec3 typed;
};

enum class FlattenTo { Average, Lowest, Highest };

// The pivot for a selection.
Vec3 pivotPoint(const Mesh& m, const Selection& s, PivotKind kind, const Vec3& typed);
// The selection's normal: the average of its faces' normals (points and edges: of the
// faces around them). Zero if there is none.
Vec3 selectionNormal(const Mesh& m, const Selection& s);

// Moves, turns and scales the selection's points. Returns false if nothing changed.
bool applyTransform(MeshEdit& e, const Selection& s, const TransformParams& p);
// N copies of the faces the selection covers; returns the copies' faces.
std::set<int> replicate(MeshEdit& e, const Selection& s, const ReplicateParams& p);
// A copy in place; returns its faces.
std::set<int> duplicate(MeshEdit& e, const Selection& s);
// Flips the selection on `axis` across its own centre.
void mirrorInPlace(MeshEdit& e, const Selection& s, int axis);
// A mirrored copy across the world origin's plane on `axis`; returns its faces.
std::set<int> mirrorClone(MeshEdit& e, const Selection& s, int axis);
// Lines the selected points up on `axis`.
void flatten(MeshEdit& e, const Selection& s, int axis, FlattenTo to);

// Turns a face round (reverses its corners, keeping each corner's texture coordinate
// and colour with it).
Face flipped(const Face& f);

} // namespace mazz
