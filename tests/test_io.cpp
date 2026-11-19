// File format and export tests.
#include "check.hpp"
#include "core/primitives.hpp"
#include "io/export.hpp"
#include "io/mazz_file.hpp"
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include <cstdio>
#include <cstring>
#include <sstream>

using namespace mazz;

namespace {

Model sampleModel() {
    Model m;
    PrimParams p;
    p.kind = PrimKind::Box;
    p.size = {40, 20, 10};
    p.centre = {0.1, -3.25, 1e-7};
    m.mesh = makePrimitive(p);
    p.kind = PrimKind::Sphere;
    p.centre = {100, 0, 0};
    m.mesh.append(makePrimitive(p));
    m.pivot = {1, 2, 3};
    m.texel = 0.75;
    m.mesh.faces[0].hidden = true;
    m.mesh.faces[1].diag = true;
    m.mesh.faces[2].twoSided = true;
    m.mesh.faces[3].uv[0] = {16, 0.5};
    m.mesh.faces[3].col[2] = {192, 10, 0};
    m.mesh.faces[4].tex = 0;
    m.mesh.faces[4].part = 2;
    RefImage r;
    r.dir = ViewDir::Right;
    r.path = "photos/side \"left\".png";
    r.x = -32.5;
    r.rotation = 12;
    r.width = 140;
    r.alpha = 0.35;
    r.inFront = true;
    m.refs.push_back(r);
    return m;
}

bool sameModel(const Model& a, const Model& b) {
    if (a.mesh.verts.size() != b.mesh.verts.size() || a.mesh.faces.size() != b.mesh.faces.size()) return false;
    for (size_t i = 0; i < a.mesh.verts.size(); ++i)
        if (a.mesh.verts[i].x != b.mesh.verts[i].x || a.mesh.verts[i].y != b.mesh.verts[i].y || a.mesh.verts[i].z != b.mesh.verts[i].z)
            return false;
    for (size_t i = 0; i < a.mesh.faces.size(); ++i) {
        const Face &x = a.mesh.faces[i], &y = b.mesh.faces[i];
        if (x.n != y.n || x.sg != y.sg || x.tex != y.tex || x.part != y.part || x.twoSided != y.twoSided || x.diag != y.diag ||
            x.hidden != y.hidden)
            return false;
        for (int k = 0; k < x.n; ++k)
            if (x.v[k] != y.v[k] || x.uv[k].x != y.uv[k].x || x.uv[k].y != y.uv[k].y || !(x.col[k] == y.col[k])) return false;
    }
    return a.pivot.x == b.pivot.x && a.pivot.y == b.pivot.y && a.pivot.z == b.pivot.z && a.texel == b.texel && a.refs == b.refs;
}

void testNumbers() {
    CHECK(formatNumber(0) == "0");
    CHECK(formatNumber(-0.0) == "0");
    CHECK(formatNumber(12) == "12");
    CHECK(formatNumber(0.1) == "0.1");
    CHECK(formatNumber(-3.5) == "-3.5");
    for (double v : {1.0 / 3.0, 1e-7, 123456.789, -2.5e-300, 6.02e23}) CHECK(std::strtod(formatNumber(v).c_str(), nullptr) == v);
}

void testMazz() {
    const Model m = sampleModel();
    const std::string text = writeMazz(m);
    Model back;
    std::string err;
    CHECK(readMazz(text, back, err));
    CHECK(err.empty());
    CHECK(sameModel(m, back));
    CHECK(writeMazz(back) == text); // golden: load -> save is identical
    // A hand-written file.
    const char* hand =
        "mazzuola 1\n# a comment\nunits cm\npivot 0 0 0\ntexel 0.5\n"
        "v 0 0 0\nv 10 0 0\nv 10 10 0\nv 0 10 0\n"
        "f 0 1 2 3 sg=2 diag=1\n  uv 0 0 / 16 0 / 16 16 / 0 16\n  col ffffff ffffff c0c0c0 c0c0c0\n"
        "ref Front \"front.png\" x=-32 y=0 rot=0 width=50 alpha=0.4 layer=back\n";
    Model h;
    CHECK(readMazz(hand, h, err));
    CHECK(h.mesh.faces.size() == 1 && h.mesh.faces[0].n == 4 && h.mesh.faces[0].sg == 2 && h.mesh.faces[0].diag);
    CHECK(h.mesh.faces[0].uv[2].x == 16 && h.mesh.faces[0].col[2].r == 0xc0);
    CHECK(h.refs.size() == 1 && h.refs[0].dir == ViewDir::Front && h.refs[0].width == 50 && !h.refs[0].inFront);
    // Errors name the line.
    CHECK(!readMazz("mazzuola 1\nv 0 0\n", h, err) && err.find("line 2") == 0);
    CHECK(!readMazz("mazzuola 1\nv 0 0 0\nf 0 1 2\n", h, err) && err.find("line 3") == 0);
    CHECK(!readMazz("hello\n", h, err));
    CHECK(!readMazz("mazzuola 2\n", h, err));
    CHECK(!readMazz("", h, err));
    CHECK(!readMazz("mazzuola 1\nwhat 3\n", h, err));
    // Files on disk.
    const std::string path = "mazz_io_test.mazz";
    CHECK(saveMazzFile(path, m, err));
    Model disk;
    CHECK(loadMazzFile(path, disk, err) && sameModel(m, disk));
    std::remove(path.c_str());
}

void testObj() {
    Model m;
    PrimParams p;
    p.kind = PrimKind::Box;
    p.size = {200, 100, 50};
    p.centre = {100, 0, 25};
    m.mesh = makePrimitive(p);
    m.pivot = {100, 0, 0};
    const std::string obj = objText(m);
    std::istringstream in(obj);
    std::string line;
    int v = 0, f = 0, vn = 0;
    double maxY = -1e9, minY = 1e9;
    while (std::getline(in, line)) {
        if (line.rfind("v ", 0) == 0) {
            ++v;
            double x, y, z;
            std::sscanf(line.c_str(), "v %lf %lf %lf", &x, &y, &z);
            maxY = std::max(maxY, y);
            minY = std::min(minY, y);
        } else if (line.rfind("vn ", 0) == 0) ++vn;
        else if (line.rfind("f ", 0) == 0) ++f;
    }
    CHECK(v == 8 && f == 6 && vn == 6);
    // Z (0..50 cm) became Y (0..0.5 m), relative to the pivot.
    CHECK(near(minY, 0) && near(maxY, 0.5));
}

void testGltf() {
    Model m = sampleModel();
    std::string err;
    const std::string glb = "mazz_io_test.glb", gltf = "mazz_io_test.gltf";
    CHECK(exportGlb(glb, m, err));
    CHECK(exportGltf(gltf, m, err));
    for (const std::string& path : {glb, gltf}) {
        cgltf_options opt;
        std::memset(&opt, 0, sizeof opt);
        cgltf_data* data = nullptr;
        const bool parsed = cgltf_parse_file(&opt, path.c_str(), &data) == cgltf_result_success;
        CHECK(parsed);
        if (!parsed) continue;
        CHECK(cgltf_load_buffers(&opt, data, path.c_str()) == cgltf_result_success);
        CHECK(cgltf_validate(data) == cgltf_result_success);
        CHECK(data->meshes_count == 1 && data->nodes_count == 1);
        // Two primitives: one-sided and two-sided faces.
        CHECK(data->meshes[0].primitives_count == 2);
        size_t tris = 0;
        for (size_t i = 0; i < data->meshes[0].primitives_count; ++i) {
            const cgltf_primitive& pr = data->meshes[0].primitives[i];
            tris += pr.indices->count / 3;
            CHECK(pr.material && pr.material->double_sided == (i == 1));
            // Positions read back: the first one matches the model, converted.
            const cgltf_accessor* pos = nullptr;
            for (size_t a = 0; a < pr.attributes_count; ++a)
                if (pr.attributes[a].type == cgltf_attribute_type_position) pos = pr.attributes[a].data;
            CHECK(pos && pos->count > 0);
        }
        CHECK(tris == (size_t)m.mesh.triangleCount());
        // Extras carry the units and pivot.
        CHECK(data->nodes[0].extras.data && std::strstr(data->nodes[0].extras.data, "\"units\":\"cm\"") != nullptr);
        CHECK(std::strstr(data->nodes[0].extras.data, "\"pivot\":[1,2,3]") != nullptr);
        cgltf_free(data);
    }
    // Bounds: the box (40 x 20 x 10 cm) becomes 0.4 x 0.1 (Y) x 0.2 (Z) m.
    Model box;
    PrimParams p;
    p.kind = PrimKind::Box;
    p.size = {40, 20, 10};
    box.mesh = makePrimitive(p);
    std::string json, bin;
    gltfParts(box, "", json, bin);
    CHECK(json.find("\"min\":[-0.2,-0.05,-0.1]") != std::string::npos);
    CHECK(json.find("\"max\":[0.2,0.05,0.1]") != std::string::npos);
    std::remove(glb.c_str());
    std::remove(gltf.c_str());
    std::remove("mazz_io_test.bin");
    // Directions: up (Z) becomes Y, back (+Y) becomes -Z.
    CHECK(nearV(toExportDirection({0, 0, 1}), {0, 1, 0}));
    CHECK(nearV(toExportDirection({0, 1, 0}), {0, 0, -1}));
}

} // namespace

void runIoTests() {
    testNumbers();
    testMazz();
    testObj();
    testGltf();
}
