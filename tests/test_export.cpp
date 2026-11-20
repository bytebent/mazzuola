// Export tests: the .glb / .gltf files read back with cgltf, the .obj parsed back.
// Mazzuola is Z-up cm; exports are Y-up metres: (x, y, z) cm -> (x, z, -y) / 100.
#include "check.hpp"
#include "core/primitives.hpp"
#include "io/export.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "../third_party/cgltf/cgltf.h" // implementation compiled in io/export.cpp

using namespace mazz;
namespace fs = std::filesystem;

namespace {

fs::path tempDir() {
    fs::path d = fs::temp_directory_path() / ("mazz_export_test_" + std::to_string(std::rand()));
    fs::create_directories(d);
    return d;
}

std::string readText(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool noLeftovers(const fs::path& dir) {
    for (const auto& e : fs::directory_iterator(dir))
        if (e.path().extension() == ".saving" || e.path().extension() == ".previous") return false;
    return true;
}

Mesh testBox() {
    Mesh m;
    addBox(m, {0, 0, 0}, {100, 50, 62.5});
    return m;
}

Vec3 v3(const float* p) { return {p[0], p[1], p[2]}; }

// Reads a .glb/.gltf back and checks it against `m` (Z-up cm).
void checkGltf(const fs::path& path, const Mesh& m) {
    cgltf_options opt{};
    cgltf_data* d = nullptr;
    CHECK(cgltf_parse_file(&opt, path.string().c_str(), &d) == cgltf_result_success);
    if (!d) return;
    CHECK(cgltf_load_buffers(&opt, d, path.string().c_str()) == cgltf_result_success);
    CHECK(cgltf_validate(d) == cgltf_result_success);
    CHECK(d->meshes_count == 1 && d->nodes_count == 1 && d->scenes_count == 1 && d->materials_count == 1);
    CHECK(d->asset.extras.data && std::string(d->asset.extras.data).find("\"sourceUnits\":\"cm\"") != std::string::npos);
    CHECK(d->materials[0].has_pbr_metallic_roughness);
    CHECK(near(d->materials[0].pbr_metallic_roughness.base_color_factor[0], 0.75, 0));
    CHECK(d->materials[0].pbr_metallic_roughness.metallic_factor == 0);
    CHECK(!d->materials[0].double_sided);
    if (d->meshes_count != 1 || d->meshes[0].primitives_count != 1) {
        cgltf_free(d);
        return;
    }
    const cgltf_primitive& pr = d->meshes[0].primitives[0];
    CHECK(pr.type == cgltf_primitive_type_triangles);
    const cgltf_accessor *pos = nullptr, *nrm = nullptr;
    for (size_t i = 0; i < pr.attributes_count; ++i) {
        if (pr.attributes[i].type == cgltf_attribute_type_position) pos = pr.attributes[i].data;
        if (pr.attributes[i].type == cgltf_attribute_type_normal) nrm = pr.attributes[i].data;
    }
    CHECK(pos && nrm && pr.indices);
    if (!pos || !nrm || !pr.indices) {
        cgltf_free(d);
        return;
    }
    int corners = 0, tris = 0;
    for (const Face& f : m.faces) corners += f.n, tris += f.n - 2;
    CHECK((int)pos->count == corners);
    CHECK((int)pr.indices->count == 3 * tris);

    // Bounds in metres, Y-up.
    Vec3 mn, mx;
    m.bounds(mn, mx);
    CHECK(pos->has_min && pos->has_max);
    CHECK(near(pos->min[0], mn.x / 100, 1e-6) && near(pos->max[0], mx.x / 100, 1e-6));
    CHECK(near(pos->min[1], mn.z / 100, 1e-6) && near(pos->max[1], mx.z / 100, 1e-6));
    CHECK(near(pos->min[2], -mx.y / 100, 1e-6) && near(pos->max[2], -mn.y / 100, 1e-6));

    // Every point is a model point converted; every triangle turns the way its normal
    // points (faces stay outward); normals are unit length.
    int bad = 0, badNormal = 0, badWinding = 0;
    for (size_t t = 0; t < pr.indices->count; t += 3) {
        float p[3][3], n[3];
        for (int k = 0; k < 3; ++k) {
            const cgltf_size i = cgltf_accessor_read_index(pr.indices, t + k);
            cgltf_accessor_read_float(pos, i, p[k], 3);
            if (k == 0) cgltf_accessor_read_float(nrm, i, n, 3);
        }
        const Vec3 a = v3(p[0]), b = v3(p[1]), c = v3(p[2]), nn = v3(n);
        if (!near(length(nn), 1, 1e-5)) ++badNormal;
        if (dot(cross(b - a, c - a), nn) <= 0) ++badWinding;
        for (int k = 0; k < 3; ++k) {
            const Vec3 back = {p[k][0] * 100.0, -p[k][2] * 100.0, p[k][1] * 100.0};
            bool found = false;
            for (const Vec3& q : m.verts)
                if (nearV(q, back, 1e-3)) found = true;
            if (!found) ++bad;
        }
    }
    CHECK(bad == 0);
    CHECK(badNormal == 0);
    CHECK(badWinding == 0);
    cgltf_free(d);
}

void testGlb() {
    const fs::path dir = tempDir();
    const Mesh m = testBox();
    std::string err;
    CHECK(exportModel((dir / "box.glb").string(), ExportFormat::Glb, m, "Mazzuola test", err));
    CHECK(err.empty());
    checkGltf(dir / "box.glb", m);
    // A box's top face (z = 62.5 cm) points up: +Y in the file.
    const ExportMesh em = buildExportMesh(m);
    int up = 0;
    for (int i = 0; i < em.vertexCount(); ++i)
        if (em.nrm[3 * i + 1] == 1) {
            ++up;
            CHECK(near(em.pos[3 * i + 1], 0.625, 1e-7));
        }
    CHECK(up == 4);
    CHECK(noLeftovers(dir));
    // Exporting again replaces the file.
    CHECK(exportModel((dir / "box.glb").string(), ExportFormat::Glb, m, "Mazzuola test", err));
    CHECK(noLeftovers(dir));
    fs::remove_all(dir);
}

void testGltfWithBin() {
    const fs::path dir = tempDir();
    const Mesh m = testBox();
    std::string err;
    CHECK(exportModel((dir / "My box.gltf").string(), ExportFormat::Gltf, m, "Mazzuola test", err));
    CHECK(fs::exists(dir / "My box.bin"));
    const std::string json = readText(dir / "My box.gltf");
    CHECK(json.find("\"uri\"") != std::string::npos);
    CHECK(json.find("Mazzuola test") != std::string::npos);
    checkGltf(dir / "My box.gltf", m);
    CHECK(noLeftovers(dir));
    // Again over the old pair: replaced, nothing left aside.
    CHECK(exportModel((dir / "My box.gltf").string(), ExportFormat::Gltf, m, "Mazzuola test", err));
    checkGltf(dir / "My box.gltf", m);
    CHECK(noLeftovers(dir));
    fs::remove_all(dir);
}

void testShapes() {
    // Triangles, quads with either diagonal, several pieces.
    const fs::path dir = tempDir();
    PrimParams p;
    p.kind = PrimKind::Sphere;
    Mesh m = makePrimitive(p, {-50, -50, 0}, {50, 50, 100}, 2);
    p.kind = PrimKind::Box;
    p.roundSegments = 2;
    const Mesh rounded = makePrimitive(p, {100, 0, 0}, {200, 100, 80}, 2);
    const int base = (int)m.verts.size();
    m.verts.insert(m.verts.end(), rounded.verts.begin(), rounded.verts.end());
    for (Face f : rounded.faces) {
        for (int k = 0; k < f.n; ++k) f.v[k] += base;
        f.diag = !f.diag;
        m.faces.push_back(f);
    }
    std::string err;
    CHECK(exportModel((dir / "shapes.glb").string(), ExportFormat::Glb, m, "t", err));
    checkGltf(dir / "shapes.glb", m);
    fs::remove_all(dir);
}

void testDiagonal() {
    Mesh m;
    m.verts = {{0, 0, 0}, {100, 0, 0}, {100, 100, 0}, {0, 100, 0}};
    Face f;
    f.n = 4;
    for (int k = 0; k < 4; ++k) f.v[k] = k;
    m.faces.push_back(f);
    ExportMesh e = buildExportMesh(m);
    CHECK((e.idx == std::vector<std::uint32_t>{0, 1, 2, 0, 2, 3}));
    m.faces[0].diag = true;
    e = buildExportMesh(m);
    CHECK((e.idx == std::vector<std::uint32_t>{0, 1, 3, 1, 2, 3}));
    const std::string obj = writeObj(m, "q", "q.mtl", "t");
    CHECK(obj.find("f 1//1 2//1 4//1\nf 2//1 3//1 4//1\n") != std::string::npos);
}

void testObj() {
    const fs::path dir = tempDir();
    const Mesh m = testBox();
    std::string err;
    CHECK(exportModel((dir / "box.obj").string(), ExportFormat::Obj, m, "Mazzuola test", err));
    CHECK(fs::exists(dir / "box.mtl"));
    CHECK(noLeftovers(dir));
    const std::string text = readText(dir / "box.obj");
    CHECK(text.find("mtllib box.mtl\n") != std::string::npos);
    CHECK(text.find("usemtl grey\n") != std::string::npos);
    CHECK(readText(dir / "box.mtl").find("newmtl grey\n") == 0);
    // Parse it back.
    std::istringstream in(text);
    std::string line;
    std::vector<Vec3> v, vn;
    int faces = 0, badWinding = 0, badIndex = 0;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string key;
        ls >> key;
        if (key == "v" || key == "vn") {
            Vec3 p;
            ls >> p.x >> p.y >> p.z;
            (key == "v" ? v : vn).push_back(p);
        } else if (key == "f") {
            ++faces;
            int vi[3], ni[3];
            for (int k = 0; k < 3; ++k) {
                std::string w;
                ls >> w;
                vi[k] = std::atoi(w.c_str());
                ni[k] = std::atoi(w.substr(w.find("//") + 2).c_str());
                if (vi[k] < 1 || vi[k] > (int)v.size() || ni[k] < 1 || ni[k] > (int)vn.size()) ++badIndex;
            }
            if (badIndex) continue;
            const Vec3 a = v[vi[0] - 1], b = v[vi[1] - 1], c = v[vi[2] - 1];
            if (dot(cross(b - a, c - a), vn[ni[0] - 1]) <= 0) ++badWinding;
        }
    }
    CHECK(v.size() == 8);
    CHECK(vn.size() == 6);
    CHECK(faces == 12);
    CHECK(badIndex == 0);
    CHECK(badWinding == 0);
    // Exact plain decimals: the corner (100, 50, 62.5) cm is (1, 0.625, -0.5) m.
    CHECK(text.find("v 1 0.625 -0.5\n") != std::string::npos);
    CHECK(text.find("e-") == std::string::npos && text.find("-0 ") == std::string::npos);
    fs::remove_all(dir);
}

void testSmallNumbers() {
    Mesh m;
    m.verts = {{0.125, -0.001, 1234.567}, {1, 0, 0}, {0, 1, 0}};
    Face f;
    f.n = 3;
    f.v[0] = 0, f.v[1] = 1, f.v[2] = 2;
    m.faces.push_back(f);
    const std::string obj = writeObj(m, "s", "s.mtl", "t");
    CHECK(obj.find("v 0.00125 12.34567 0.00001\n") != std::string::npos);
}

void testFormats() {
    ExportFormat f;
    CHECK(exportFormatFromPath("a/b.GLB", f) && f == ExportFormat::Glb);
    CHECK(exportFormatFromPath("x.gltf", f) && f == ExportFormat::Gltf);
    CHECK(exportFormatFromPath("x.Obj", f) && f == ExportFormat::Obj);
    CHECK(!exportFormatFromPath("x.mazz", f));
    std::string err;
    CHECK(!exportModel("never.glb", ExportFormat::Glb, Mesh(), "t", err));
    CHECK(!err.empty());
    CHECK(!fs::exists("never.glb"));
}

void writeText(const fs::path& p, const std::string& s) {
    std::ofstream f(p, std::ios::binary);
    f << s;
}

void testCommitAllOrNothing() {
    // The .bin goes in first; if the .gltf then fails, the old .bin must come back,
    // never an old .gltf next to a new .bin.
    const fs::path dir = tempDir();
    const fs::path bin = dir / "m.bin", gltf = dir / "m.gltf";
    writeText(bin, "old bin");
    writeText(gltf, "old gltf");
    writeText(dir / "m.bin.saving", "new bin"); // no m.gltf.saving: its rename fails
    const std::vector<std::pair<std::string, std::string>> files = {
        {(dir / "m.bin.saving").string(), bin.string()}, {(dir / "m.gltf.saving").string(), gltf.string()}};
    std::string err;
    CHECK(!commitExportFiles(files, err));
    CHECK(!err.empty());
    CHECK(readText(bin) == "old bin");
    CHECK(readText(gltf) == "old gltf");
    CHECK(noLeftovers(dir));

    // A first export (no old files) that fails leaves nothing behind.
    fs::remove(bin);
    fs::remove(gltf);
    writeText(dir / "m.bin.saving", "new bin");
    CHECK(!commitExportFiles(files, err));
    CHECK(!fs::exists(bin) && !fs::exists(gltf));
    CHECK(noLeftovers(dir));

    // Both in place: the new pair replaces the old one.
    writeText(bin, "old bin");
    writeText(gltf, "old gltf");
    writeText(dir / "m.bin.saving", "new bin");
    writeText(dir / "m.gltf.saving", "new gltf");
    CHECK(commitExportFiles(files, err));
    CHECK(readText(bin) == "new bin");
    CHECK(readText(gltf) == "new gltf");
    CHECK(noLeftovers(dir));
    fs::remove_all(dir);
}

} // namespace

void testExport() {
    testGlb();
    testGltfWithBin();
    testShapes();
    testDiagonal();
    testObj();
    testSmallNumbers();
    testFormats();
    testCommitAllOrNothing();
}
