#include "io/export.hpp"
#include "core/numbers.hpp"
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <tuple>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-function"
#if !defined(__clang__)
#pragma GCC diagnostic ignored "-Wformat-truncation" // cgltf measures with snprintf(NULL, 0, ...)
#endif
#endif
// cgltf (reading, used by the tests) and its writer, compiled here once.
#define CGLTF_IMPLEMENTATION
#include "../../third_party/cgltf/cgltf.h"
#undef CGLTF_IMPLEMENTATION
#define CGLTF_WRITE_IMPLEMENTATION
#include "../../third_party/cgltf/cgltf_write.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace mazz {

namespace fs = std::filesystem;

namespace {

constexpr float kGrey = 0.75f; // exact in binary, so the file says 0.75

// Z-up cm -> Y-up m.
void toExport(const Vec3& p, double scale, float out[3]) {
    out[0] = (float)(p.x * scale);
    out[1] = (float)(p.z * scale);
    out[2] = (float)(-p.y * scale);
    for (int k = 0; k < 3; ++k)
        if (out[k] == 0) out[k] = 0; // no -0
}

// Corner order of a face's triangles, as drawn (core/draw_data.cpp).
int triCorners(const Face& f, int out[6]) {
    if (f.n == 3) {
        out[0] = 0, out[1] = 1, out[2] = 2;
        return 3;
    }
    const int a[6] = {0, 1, 2, 0, 2, 3}, b[6] = {0, 1, 3, 1, 2, 3};
    std::memcpy(out, f.diag ? b : a, sizeof a);
    return 6;
}

// A clean cm value as metres in plain decimal text, exactly ("62.5" cm -> "0.625").
std::string metresText(double cm) {
    long long t = std::llround(clean(cm) * kCleanSteps); // thousandths of a cm
    const bool neg = t < 0;
    if (neg) t = -t;
    const long long whole = t / 100000, frac = t % 100000; // 1 m = 100000 thousandths of a cm
    std::string s = (neg ? "-" : "") + std::to_string(whole);
    if (frac) {
        char buf[8];
        std::snprintf(buf, sizeof buf, "%05lld", frac);
        std::string f = buf;
        while (!f.empty() && f.back() == '0') f.pop_back();
        s += "." + f;
    }
    return s;
}

std::string normalText(double v) {
    if (std::fabs(v) < 5e-7) return "0";
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.6f", v);
    std::string s = buf;
    while (s.back() == '0') s.pop_back();
    if (s.back() == '.') s.pop_back();
    return s;
}

std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

bool writeFile(const std::string& path, const std::string& data, std::string& error) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) {
        error = "can't write there";
        return false;
    }
    f.write(data.data(), (std::streamsize)data.size());
    f.flush();
    if (!f) {
        error = "writing failed (disk full?)";
        return false;
    }
    return true;
}

} // namespace

// Moves each existing final file aside first, then renames every temporary into place.
// If any step fails, the files already moved in are removed and the old ones put back,
// so a .gltf and its .bin (or an .obj and its .mtl) are always from the same export.
bool commitExportFiles(const std::vector<std::pair<std::string, std::string>>& files,
                       std::string& error) {
    std::vector<std::string> backups(files.size()); // empty: no old file
    size_t placed = 0;
    auto undo = [&](size_t backedUp) {
        for (size_t i = 0; i < placed; ++i) std::remove(files[i].second.c_str());
        for (size_t i = 0; i < backedUp; ++i)
            if (!backups[i].empty()) std::rename(backups[i].c_str(), files[i].second.c_str());
        for (const auto& f : files) std::remove(f.first.c_str());
    };
    // Only plain files get replaced: a folder (or anything else) with the name is left alone.
    for (const auto& f : files) {
        std::error_code ec;
        const fs::file_status st = fs::symlink_status(f.second, ec);
        if (fs::exists(st) && !fs::is_regular_file(st)) {
            error = "there is a folder or other non-file named " + fs::path(f.second).filename().string();
            for (const auto& g : files) std::remove(g.first.c_str());
            return false;
        }
    }
    for (size_t i = 0; i < files.size(); ++i) {
        std::error_code ec;
        if (!fs::exists(files[i].second, ec)) continue;
        backups[i] = files[i].second + ".previous";
        if (std::rename(files[i].second.c_str(), backups[i].c_str()) != 0) {
            error = std::string("can't replace the file: ") + std::strerror(errno);
            backups[i].clear();
            undo(i);
            return false;
        }
    }
    for (; placed < files.size(); ++placed)
        if (std::rename(files[placed].first.c_str(), files[placed].second.c_str()) != 0) {
            error = std::string("can't replace the file: ") + std::strerror(errno);
            undo(files.size());
            return false;
        }
    for (const auto& b : backups)
        if (!b.empty()) std::remove(b.c_str());
    return true;
}

namespace {

bool exportGltf(const std::string& path, bool binary, const Mesh& m, const std::string& generator,
                std::string& error) {
    const ExportMesh em = buildExportMesh(m);
    const size_t posBytes = em.pos.size() * sizeof(float), nrmBytes = em.nrm.size() * sizeof(float),
                 idxBytes = em.idx.size() * sizeof(std::uint32_t);
    std::string bin(posBytes + nrmBytes + idxBytes, '\0');
    std::memcpy(&bin[0], em.pos.data(), posBytes);
    std::memcpy(&bin[posBytes], em.nrm.data(), nrmBytes);
    std::memcpy(&bin[posBytes + nrmBytes], em.idx.data(), idxBytes);

    const fs::path target(path);
    std::string binName = target.stem().string() + ".bin";
    std::string name = target.stem().string();
    std::string gen = generator;
    char extras[] = "{\"sourceUnits\":\"cm\",\"sourceUp\":\"Z\"}";
    char matName[] = "grey";
    char attrPos[] = "POSITION", attrNrm[] = "NORMAL";

    cgltf_buffer buffer{};
    buffer.size = bin.size();
    if (!binary) buffer.uri = &binName[0];

    cgltf_buffer_view views[3]{};
    const size_t offs[3] = {0, posBytes, posBytes + nrmBytes}, sizes[3] = {posBytes, nrmBytes, idxBytes};
    for (int i = 0; i < 3; ++i) {
        views[i].buffer = &buffer;
        views[i].offset = offs[i];
        views[i].size = sizes[i];
        views[i].type = i < 2 ? cgltf_buffer_view_type_vertices : cgltf_buffer_view_type_indices;
    }

    cgltf_accessor acc[3]{};
    acc[0].component_type = cgltf_component_type_r_32f;
    acc[0].type = cgltf_type_vec3;
    acc[0].count = em.vertexCount();
    acc[0].buffer_view = &views[0];
    acc[0].has_min = acc[0].has_max = 1;
    for (int k = 0; k < 3; ++k) acc[0].min[k] = em.mn[k], acc[0].max[k] = em.mx[k];
    acc[1].component_type = cgltf_component_type_r_32f;
    acc[1].type = cgltf_type_vec3;
    acc[1].count = em.vertexCount();
    acc[1].buffer_view = &views[1];
    acc[2].component_type = cgltf_component_type_r_32u;
    acc[2].type = cgltf_type_scalar;
    acc[2].count = em.idx.size();
    acc[2].buffer_view = &views[2];

    cgltf_material mat{};
    mat.name = matName;
    mat.has_pbr_metallic_roughness = 1;
    for (int k = 0; k < 3; ++k) mat.pbr_metallic_roughness.base_color_factor[k] = kGrey;
    mat.pbr_metallic_roughness.base_color_factor[3] = 1;
    mat.pbr_metallic_roughness.metallic_factor = 0;
    mat.pbr_metallic_roughness.roughness_factor = 1;

    cgltf_attribute attrs[2]{};
    attrs[0].name = attrPos;
    attrs[0].type = cgltf_attribute_type_position;
    attrs[0].data = &acc[0];
    attrs[1].name = attrNrm;
    attrs[1].type = cgltf_attribute_type_normal;
    attrs[1].data = &acc[1];

    cgltf_primitive prim{};
    prim.type = cgltf_primitive_type_triangles;
    prim.indices = &acc[2];
    prim.material = &mat;
    prim.attributes = attrs;
    prim.attributes_count = 2;

    cgltf_mesh mesh{};
    mesh.name = &name[0];
    mesh.primitives = &prim;
    mesh.primitives_count = 1;

    cgltf_node node{};
    node.name = &name[0];
    node.mesh = &mesh;
    cgltf_node* nodes[] = {&node};

    cgltf_scene scene{};
    scene.nodes = nodes;
    scene.nodes_count = 1;

    cgltf_data data{};
    data.asset.version = const_cast<char*>("2.0");
    data.asset.generator = &gen[0];
    data.asset.extras.data = extras;
    data.buffers = &buffer;
    data.buffers_count = 1;
    data.buffer_views = views;
    data.buffer_views_count = 3;
    data.accessors = acc;
    data.accessors_count = 3;
    data.materials = &mat;
    data.materials_count = 1;
    data.meshes = &mesh;
    data.meshes_count = 1;
    data.nodes = &node;
    data.nodes_count = 1;
    data.scenes = &scene;
    data.scenes_count = 1;
    data.scene = &scene;
    if (binary) {
        data.bin = bin.data();
        data.bin_size = bin.size();
    }

    cgltf_options opt{};
    opt.type = binary ? cgltf_file_type_glb : cgltf_file_type_gltf;
    const std::string tmp = path + ".saving";
    std::vector<std::pair<std::string, std::string>> files;
    if (!binary) {
        const std::string binPath = (target.parent_path() / binName).string();
        if (!writeFile(binPath + ".saving", bin, error)) {
            std::remove((binPath + ".saving").c_str());
            return false;
        }
        files.push_back({binPath + ".saving", binPath});
    }
    if (cgltf_write_file(&opt, tmp.c_str(), &data) != cgltf_result_success) {
        error = "can't write there";
        std::remove(tmp.c_str());
        for (auto& f : files) std::remove(f.first.c_str());
        return false;
    }
    files.push_back({tmp, path});
    return commitExportFiles(files, error);
}

bool exportObj(const std::string& path, const Mesh& m, const std::string& generator, std::string& error) {
    const fs::path target(path);
    const std::string mtlName = target.stem().string() + ".mtl";
    const std::string mtlPath = (target.parent_path() / mtlName).string();
    const std::string tmp = path + ".saving", mtlTmp = mtlPath + ".saving";
    if (!writeFile(mtlTmp, writeMtl(), error)) {
        std::remove(mtlTmp.c_str());
        return false;
    }
    if (!writeFile(tmp, writeObj(m, target.stem().string(), mtlName, generator), error)) {
        std::remove(tmp.c_str());
        std::remove(mtlTmp.c_str());
        return false;
    }
    return commitExportFiles({{mtlTmp, mtlPath}, {tmp, path}}, error);
}

} // namespace

const char* exportExtension(ExportFormat f) {
    switch (f) {
    case ExportFormat::Glb: return "glb";
    case ExportFormat::Gltf: return "gltf";
    case ExportFormat::Obj: return "obj";
    }
    return "";
}

const char* exportFormatName(ExportFormat f) {
    switch (f) {
    case ExportFormat::Glb: return "glTF binary (.glb)";
    case ExportFormat::Gltf: return "glTF (.gltf + .bin)";
    case ExportFormat::Obj: return "Wavefront (.obj + .mtl)";
    }
    return "";
}

bool exportFormatFromPath(const std::string& path, ExportFormat& out) {
    const std::string ext = lower(fs::path(path).extension().string());
    if (ext == ".glb") out = ExportFormat::Glb;
    else if (ext == ".gltf") out = ExportFormat::Gltf;
    else if (ext == ".obj") out = ExportFormat::Obj;
    else return false;
    return true;
}

ExportMesh buildExportMesh(const Mesh& m) {
    ExportMesh e;
    bool first = true;
    for (int fi = 0; fi < (int)m.faces.size(); ++fi) {
        const Face& f = m.faces[fi];
        float n[3];
        toExport(faceNormal(m, fi), 1.0, n);
        const std::uint32_t base = (std::uint32_t)e.vertexCount();
        for (int k = 0; k < f.n; ++k) {
            float p[3];
            toExport(m.verts[f.v[k]], 0.01, p);
            for (int a = 0; a < 3; ++a) {
                e.pos.push_back(p[a]);
                e.nrm.push_back(n[a]);
                if (first || p[a] < e.mn[a]) e.mn[a] = p[a];
                if (first || p[a] > e.mx[a]) e.mx[a] = p[a];
            }
            first = false;
        }
        int c[6];
        const int count = triCorners(f, c);
        for (int i = 0; i < count; ++i) e.idx.push_back(base + (std::uint32_t)c[i]);
    }
    return e;
}

std::string writeObj(const Mesh& m, const std::string& objectName, const std::string& mtlName,
                     const std::string& generator) {
    std::ostringstream o;
    o << "# " << generator << "\n# Y-up, metres\n";
    o << "mtllib " << mtlName << "\n";
    o << "o " << (objectName.empty() ? std::string("model") : objectName) << "\n";
    // Points used by faces, in order of first use.
    std::vector<int> number(m.verts.size(), -1);
    int count = 0;
    for (const Face& f : m.faces)
        for (int k = 0; k < f.n; ++k)
            if (number[f.v[k]] < 0) {
                number[f.v[k]] = ++count; // OBJ counts from 1
                const Vec3& p = m.verts[f.v[k]];
                o << "v " << metresText(p.x) << " " << metresText(p.z) << " " << metresText(-p.y) << "\n";
            }
    // One normal per distinct face direction.
    std::map<std::tuple<std::string, std::string, std::string>, int> normals;
    std::vector<int> faceNormal_(m.faces.size());
    for (int fi = 0; fi < (int)m.faces.size(); ++fi) {
        const Vec3 n = faceNormal(m, fi);
        const auto key = std::make_tuple(normalText(n.x), normalText(n.z), normalText(-n.y));
        auto it = normals.find(key);
        if (it == normals.end()) {
            it = normals.emplace(key, (int)normals.size() + 1).first;
            o << "vn " << std::get<0>(key) << " " << std::get<1>(key) << " " << std::get<2>(key) << "\n";
        }
        faceNormal_[fi] = it->second;
    }
    o << "usemtl grey\ns off\n";
    for (int fi = 0; fi < (int)m.faces.size(); ++fi) {
        const Face& f = m.faces[fi];
        int c[6];
        const int n = triCorners(f, c);
        for (int t = 0; t < n; t += 3) {
            o << "f";
            for (int i = 0; i < 3; ++i) o << " " << number[f.v[c[t + i]]] << "//" << faceNormal_[fi];
            o << "\n";
        }
    }
    return o.str();
}

std::string writeMtl() {
    return "newmtl grey\n"
           "Ka 0 0 0\n"
           "Kd 0.75 0.75 0.75\n"
           "Ks 0 0 0\n"
           "d 1\n"
           "illum 1\n";
}

bool exportModel(const std::string& path, ExportFormat f, const Mesh& m, const std::string& generator,
                 std::string& error) {
    if (m.faces.empty()) {
        error = "the model is empty";
        return false;
    }
    switch (f) {
    case ExportFormat::Glb: return exportGltf(path, true, m, generator, error);
    case ExportFormat::Gltf: return exportGltf(path, false, m, generator, error);
    case ExportFormat::Obj: return exportObj(path, m, generator, error);
    }
    return false;
}

} // namespace mazz
