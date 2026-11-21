// The Mesh menu's clean-up commands (ARCHITECTURE §6, v0.2.5) and what Delete does in
// point, edge and face mode. Each is one undo step; when it can't be done, a message says
// why and nothing changes.
#include "app/editor.hpp"
#include "core/clean_ops.hpp"
#include "core/numbers.hpp"
#include "core/select_ops.hpp"

namespace mazz {

void Editor::cleanUp(Act a) {
    cancelTool();
    if (doc_.sel.empty()) return;
    const Selection was = doc_.sel;
    const Mesh& m = doc_.mesh;
    const std::set<int> faces = convertSelection(m, was, SelectMode::Face).faces;
    std::set<int> verts = was.mode == SelectMode::Vert ? was.verts : convertSelection(m, was, SelectMode::Vert).verts;
    const std::set<Edge> edges = was.mode == SelectMode::Edge ? was.edges : convertSelection(m, was, SelectMode::Edge).edges;
    std::string why, title = actionInfo(a).label;
    auto selectFaces = [](Selection& sel, const std::set<int>& f) {
        sel.clear();
        sel.mode = SelectMode::Face;
        sel.faces = f;
    };
    bool ok = false;
    switch (a) {
    case Act::MergeCentre:
    case Act::MergeFirst: {
        const int first = a == Act::MergeFirst ? doc_.firstVert() : -1;
        if (verts.size() < 2) {
            why = "select two or more points";
            break;
        }
        Vec3 at{0, 0, 0};
        if (first >= 0) at = m.verts[first];
        else {
            for (int v : verts) at = at + m.verts[v];
            at = at * (1.0 / verts.size());
        }
        ok = doc_.apply(title, [&](MeshEdit& e, Selection& sel) {
            if (!mergePoints(e, verts, clean(at), first)) return false;
            sel.clear();
            sel.mode = SelectMode::Vert;
            for (int v = 0; v < (int)e.mesh().verts.size(); ++v)
                if (length(e.mesh().verts[v] - clean(at)) < 1e-9) sel.verts.insert(v);
            return true;
        });
        if (!ok) why = "it would open the model";
        break;
    }
    case Act::Collapse:
        if (edges.empty()) {
            why = "select edges";
            break;
        }
        ok = doc_.apply(title, [&](MeshEdit& e, Selection& sel) {
            if (!collapseEdges(e, edges)) return false;
            sel.clear();
            sel.mode = SelectMode::Vert;
            return true;
        });
        if (!ok) why = "it would open the model";
        break;
    case Act::Dissolve:
        ok = doc_.apply(title, [&](MeshEdit& e, Selection& sel) {
            std::set<int> out;
            const bool done = was.mode == SelectMode::Vert   ? dissolvePoints(e, was.verts, &why)
                              : was.mode == SelectMode::Edge ? dissolveEdges(e, was.edges, &why)
                                                             : dissolveFaces(e, was.faces, &out, &why);
            if (!done) return false;
            const SelectMode mode = sel.mode;
            sel.clear();
            sel.mode = mode;
            if (mode == SelectMode::Face) sel.faces = out;
            return true;
        });
        break;
    case Act::TearOpen: {
        // Faces, or the faces that use the selected points or edges: a hole is left.
        const std::set<int> gone = facesToDelete(m, was);
        ok = doc_.apply(was.mode == SelectMode::Piece ? "Delete" : title, [&](MeshEdit& e, Selection& sel) {
            e.deleteFaces(gone);
            const SelectMode mode = sel.mode;
            sel.clear();
            sel.mode = mode;
            return true;
        });
        break;
    }
    case Act::Fill:
        ok = doc_.apply(title, [&](MeshEdit& e, Selection& sel) {
            int made = -1;
            if (!fillFace(e, verts, &made, &why)) return false;
            selectFaces(sel, {made});
            return true;
        });
        break;
    case Act::Detach:
        if (faces.empty()) why = "select faces";
        else {
            ok = doc_.apply(title, [&](MeshEdit& e, Selection& sel) {
                if (!detachFaces(e, faces)) return false;
                selectFaces(sel, faces);
                return true;
            });
            if (!ok) why = "they aren't joined to anything";
        }
        break;
    case Act::FlipFaces:
    case Act::FlipDiagonal:
    case Act::Triangulate:
    case Act::JoinQuads:
    case Act::TwoSidedOn:
    case Act::TwoSidedOff:
        if (faces.empty()) {
            why = "select faces";
            break;
        }
        ok = doc_.apply(title, [&](MeshEdit& e, Selection& sel) {
            std::set<int> out = faces;
            bool done = false;
            switch (a) {
            case Act::FlipFaces: done = flipFaces(e, faces); break;
            case Act::FlipDiagonal:
                done = flipDiagonals(e, faces);
                if (!done) why = "select faces with four corners";
                break;
            case Act::Triangulate:
                done = triangulate(e, faces, &out);
                if (!done) why = "they are triangles already";
                break;
            case Act::JoinQuads:
                done = joinIntoQuads(e, faces, &out) > 0;
                if (!done) why = "no two flat triangles side by side make a good quad";
                break;
            case Act::TwoSidedOn:
            case Act::TwoSidedOff: done = setTwoSided(e, faces, a == Act::TwoSidedOn); break;
            default: break;
            }
            if (!done) return false;
            if (was.mode == SelectMode::Face) selectFaces(sel, out);
            else if (was.mode == SelectMode::Piece) sel.faces = out;
            return true;
        });
        break;
    default: return;
    }
    if (!ok) {
        if (why.empty()) why = "it can't be done here";
        showMessage(title, "Can't: " + why + ".");
    }
    touch();
}

} // namespace mazz
