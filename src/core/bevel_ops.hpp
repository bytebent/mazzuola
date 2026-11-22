#pragma once
// v0.2.6 (ARCHITECTURE §6 Bevel, Bridge, Shell; agreed 2026-11-22). Each works through a
// MeshEdit and keeps faces facing outward: a closed surface stays closed. They return
// false with a reason in `why` (and the caller reverts) when the result would be
// invalid, e.g. a bevel too wide for the faces beside it.
#include "core/document.hpp"
#include "core/selection.hpp"
#include <set>
#include <string>
#include <vector>

namespace mazz {

// ---- bevel
struct BevelParams {
    double width = 4; // cm, measured across the faces beside the edge (points: along each edge)
    int segments = 1; // 1-3; more make a round profile (edges only)
};
// Edges with a face on both sides, of a set of edges.
std::set<Edge> innerEdges(const Mesh& m, const std::set<Edge>& edges);
// Each edge becomes a strip of `segments` faces `width` from it on both sides. Where
// bevelled edges meet, a corner patch fills in (a triangle where three meet on a box).
// `out` gets the new faces.
bool bevelEdges(MeshEdit& e, const std::set<Edge>& edges, const BevelParams& p, std::set<int>* out = nullptr,
                std::string* why = nullptr);
// Cuts the corner off at each point: a new face whose corners are `width` along each
// of the point's edges. `out` gets the new faces.
bool bevelPoints(MeshEdit& e, const std::set<int>& verts, double width, std::set<int>* out = nullptr,
                 std::string* why = nullptr);

// ---- bridge
struct BridgeParams {
    int segments = 1; // rings of faces along the tube
    int twist = 0;    // steps the far end is turned by
};
// Two blocks of faces (each joined by edges; one face is a block) are removed and their
// outlines joined by a tube. `out` gets the tube's faces.
bool bridgeFaces(MeshEdit& e, const std::set<int>& faces, const BridgeParams& p, std::set<int>* out = nullptr,
                 std::string* why = nullptr);
// The two open holes the selected edges touch are joined by a tube.
bool bridgeHoles(MeshEdit& e, const std::set<Edge>& edges, const BridgeParams& p, std::set<int>* out = nullptr,
                 std::string* why = nullptr);

// ---- shell
enum class ShellSide { Inward, Outward, Centred };
struct ShellParams {
    double thickness = 2; // cm
    ShellSide side = ShellSide::Inward;
};
// The open pieces the faces belong to get a thickness: a turned-round copy offset along
// the surface, and a rim along the open edges. A closed piece is refused. `out` gets
// all faces of the shelled pieces.
bool shell(MeshEdit& e, const std::set<int>& faces, const ShellParams& p, std::set<int>* out = nullptr,
           std::string* why = nullptr);

} // namespace mazz
