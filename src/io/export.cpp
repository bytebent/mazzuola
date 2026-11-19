#include "io/export.hpp"
#include "core/ops.hpp"
#include "io/mazz_file.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <tuple>
#include <vector>

#ifndef MAZZ_VERSION
#define MAZZ_VERSION "dev"
#endif

namespace mazz {

Vec3 toExportSpace(const Vec3& p, const Vec3& pivot) {
    const Vec3 d = p - pivot;
    return Vec3{d.x, d.z, -d.y} / 100.0;
}

Vec3 toExportDirection(const Vec3& d) { return {d.x, d.z, -d.y}; }

namespace {

std::string fileName(const std::string& path) {
    const size_t s = path.find_last_of("/\\");
    return s == std::string::npos ? path : path.substr(s + 1);
}

std::string stem(const std::string& path) {
    std::string n = fileName(path);
    const size_t dot = n.find_last_of('.');
    return dot == std::string::npos ? n : n.substr(0, dot);
}

// Shortest text that reads back as the same float (accessor bounds must match the data).
std::string formatFloat(float v) {
    if (v == 0) return "0";
    char buf[32];
    for (int prec = 1; prec <= 9; ++prec) {
        std::snprintf(buf, sizeof buf, "%.*g", prec, (double)v);
        if (std::strtof(buf, nullptr) == v) break;
    }
    return buf;
}

std::string jsonEscape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if ((unsigned char)c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); o += b; }
        else o += c;
    }
    return o;
}

bool writeFile(const std::string& path, const std::string& data, std::string& error) {
    std::ofstream f(path, std::ios::binary);
    if (!f) { error = "can't write " + path; return false; }
    f.write(data.data(), (std::streamsize)data.size());
    if (!f) { error = "can't write " + path; return false; }
    return true;
}

// Corners of a face in the order that puts its diagonal at 0-2 (what importers that fan
// from the first corner will use).
std::array<int, 4> cornerOrder(const Face& f) {
    if (f.n == 4 && f.diag) return {1, 2, 3, 0};
    return {0, 1, 2, 3};
}

} // namespace

// ---------------------------------------------------------------- OBJ

std::string objText(const Model& m) {
    std::ostringstream o;
    o << "# Mazzuola " << MAZZ_VERSION << "\n# Y up, metres, origin at the model's pivot\n";
    const Mesh& mesh = m.mesh;
    for (const Vec3& v : mesh.verts) {
        const Vec3 p = toExportSpace(v, m.pivot);
        o << "v " << formatNumber(p.x) << ' ' << formatNumber(p.y) << ' ' << formatNumber(p.z) << '\n';
    }
    // Normals, shared where equal.
    const auto normals = cornerNormals(mesh);
    std::map<std::tuple<double, double, double>, int> nIndex;
    std::vector<std::array<int, 4>> faceN(mesh.faces.size());
    for (size_t f = 0; f < mesh.faces.size(); ++f)
        for (int k = 0; k < mesh.faces[f].n; ++k) {
            const Vec3 n = toExportDirection(normals[f][k]);
            const auto key = std::make_tuple(n.x, n.y, n.z);
            auto it = nIndex.find(key);
            if (it == nIndex.end()) {
                it = nIndex.emplace(key, (int)nIndex.size() + 1).first;
                o << "vn " << formatNumber(n.x) << ' ' << formatNumber(n.y) << ' ' << formatNumber(n.z) << '\n';
            }
            faceN[f][k] = it->second;
        }
    int lastSg = -1;
    for (size_t f = 0; f < mesh.faces.size(); ++f) {
        const Face& fc = mesh.faces[f];
        const int sg = fc.sg;
        if (sg != lastSg) {
            o << "s " << (sg ? std::to_string(sg) : std::string("off")) << '\n';
            lastSg = sg;
        }
        const std::array<int, 4> order = cornerOrder(fc);
        o << 'f';
        for (int k = 0; k < fc.n; ++k) o << ' ' << (fc.v[order[k]] + 1) << "//" << faceN[f][order[k]];
        o << '\n';
    }
    return o.str();
}

bool exportObj(const std::string& path, const Model& m, std::string& error) {
    return writeFile(path, objText(m), error);
}

// ---------------------------------------------------------------- glTF

void gltfParts(const Model& m, const std::string& binUri, std::string& json, std::string& bin) {
    const Mesh& mesh = m.mesh;
    const auto normals = cornerNormals(mesh);
    // Two primitives at most: one-sided faces and two-sided faces (different materials).
    struct Prim {
        std::vector<float> pos, nrm;
        std::vector<uint32_t> idx;
        std::map<std::tuple<int, float, float, float>, uint32_t> vmap;
        float mn[3] = {1e30f, 1e30f, 1e30f}, mx[3] = {-1e30f, -1e30f, -1e30f};
    } prims[2];
    for (size_t f = 0; f < mesh.faces.size(); ++f) {
        const Face& fc = mesh.faces[f];
        Prim& pr = prims[fc.twoSided ? 1 : 0];
        uint32_t corner[4];
        for (int k = 0; k < fc.n; ++k) {
            const Vec3 n = toExportDirection(normals[f][k]);
            const auto key = std::make_tuple(fc.v[k], (float)n.x, (float)n.y, (float)n.z);
            auto it = pr.vmap.find(key);
            if (it == pr.vmap.end()) {
                const Vec3 p = toExportSpace(mesh.verts[fc.v[k]], m.pivot);
                const float pf[3] = {(float)p.x, (float)p.y, (float)p.z};
                for (int a = 0; a < 3; ++a) {
                    pr.pos.push_back(pf[a]);
                    pr.mn[a] = std::min(pr.mn[a], pf[a]);
                    pr.mx[a] = std::max(pr.mx[a], pf[a]);
                }
                pr.nrm.push_back((float)n.x);
                pr.nrm.push_back((float)n.y);
                pr.nrm.push_back((float)n.z);
                it = pr.vmap.emplace(key, (uint32_t)(pr.pos.size() / 3 - 1)).first;
            }
            corner[k] = it->second;
        }
        std::array<std::array<int, 3>, 2> tris;
        const int nt = fc.triangles(tris);
        for (int t = 0; t < nt; ++t)
            for (int c = 0; c < 3; ++c) pr.idx.push_back(corner[tris[t][c]]);
    }

    // Binary buffer: per primitive positions, normals, indices (4-byte aligned).
    bin.clear();
    auto append = [&](const void* data, size_t bytes) {
        const size_t off = bin.size();
        bin.append((const char*)data, bytes);
        while (bin.size() % 4) bin.push_back('\0');
        return off;
    };
    std::ostringstream views, accessors, primitives, materials;
    int viewCount = 0, accCount = 0, primCount = 0, matCount = 0;
    for (int which = 0; which < 2; ++which) {
        Prim& pr = prims[which];
        if (pr.idx.empty()) continue;
        const uint32_t vcount = (uint32_t)(pr.pos.size() / 3);
        const bool small = vcount <= 65535;
        std::vector<uint16_t> idx16;
        if (small)
            for (uint32_t i : pr.idx) idx16.push_back((uint16_t)i);
        const size_t posOff = append(pr.pos.data(), pr.pos.size() * 4);
        const size_t nrmOff = append(pr.nrm.data(), pr.nrm.size() * 4);
        const size_t idxBytes = small ? idx16.size() * 2 : pr.idx.size() * 4;
        const size_t idxOff = small ? append(idx16.data(), idxBytes) : append(pr.idx.data(), idxBytes);
        auto view = [&](size_t off, size_t len, int target) {
            if (viewCount) views << ',';
            views << "{\"buffer\":0,\"byteOffset\":" << off << ",\"byteLength\":" << len << ",\"target\":" << target << '}';
            return viewCount++;
        };
        const int vPos = view(posOff, pr.pos.size() * 4, 34962);
        const int vNrm = view(nrmOff, pr.nrm.size() * 4, 34962);
        const int vIdx = view(idxOff, idxBytes, 34963);
        auto acc = [&](const std::string& body) {
            if (accCount) accessors << ',';
            accessors << '{' << body << '}';
            return accCount++;
        };
        std::ostringstream minmax;
        minmax << "\"min\":[" << formatFloat(pr.mn[0]) << ',' << formatFloat(pr.mn[1]) << ',' << formatFloat(pr.mn[2])
               << "],\"max\":[" << formatFloat(pr.mx[0]) << ',' << formatFloat(pr.mx[1]) << ',' << formatFloat(pr.mx[2]) << ']';
        const int aPos = acc("\"bufferView\":" + std::to_string(vPos) + ",\"componentType\":5126,\"count\":" +
                             std::to_string(vcount) + ",\"type\":\"VEC3\"," + minmax.str());
        const int aNrm = acc("\"bufferView\":" + std::to_string(vNrm) + ",\"componentType\":5126,\"count\":" +
                             std::to_string(vcount) + ",\"type\":\"VEC3\"");
        const int aIdx = acc("\"bufferView\":" + std::to_string(vIdx) + ",\"componentType\":" + (small ? "5123" : "5125") +
                             ",\"count\":" + std::to_string(pr.idx.size()) + ",\"type\":\"SCALAR\"");
        if (matCount) materials << ',';
        materials << "{\"name\":\"" << (which ? "two-sided" : "default") << "\",\"pbrMetallicRoughness\":{\"baseColorFactor\":[1,1,1,1],"
                  << "\"metallicFactor\":0,\"roughnessFactor\":1}" << (which ? ",\"doubleSided\":true" : "") << '}';
        const int mat = matCount++;
        if (primCount) primitives << ',';
        primitives << "{\"attributes\":{\"POSITION\":" << aPos << ",\"NORMAL\":" << aNrm << "},\"indices\":" << aIdx
                   << ",\"material\":" << mat << ",\"mode\":4}";
        ++primCount;
    }

    std::ostringstream j;
    j << "{\"asset\":{\"version\":\"2.0\",\"generator\":\"Mazzuola " << jsonEscape(MAZZ_VERSION) << "\"},";
    j << "\"scene\":0,\"scenes\":[{\"nodes\":[0]}],";
    j << "\"nodes\":[{\"name\":\"model\"" << (primCount ? ",\"mesh\":0" : "")
      << ",\"extras\":{\"mazzuola\":{\"units\":\"cm\",\"up\":\"z\",\"pivot\":[" << formatNumber(m.pivot.x) << ','
      << formatNumber(m.pivot.y) << ',' << formatNumber(m.pivot.z) << "]}}}]";
    if (primCount) {
        j << ",\"meshes\":[{\"name\":\"model\",\"primitives\":[" << primitives.str() << "]}]";
        j << ",\"materials\":[" << materials.str() << ']';
        j << ",\"accessors\":[" << accessors.str() << ']';
        j << ",\"bufferViews\":[" << views.str() << ']';
        j << ",\"buffers\":[{\"byteLength\":" << bin.size();
        if (!binUri.empty()) j << ",\"uri\":\"" << jsonEscape(binUri) << '"';
        j << "}]";
    }
    j << '}';
    json = j.str();
}

bool exportGlb(const std::string& path, const Model& m, std::string& error) {
    std::string json, bin;
    gltfParts(m, "", json, bin);
    while (json.size() % 4) json.push_back(' ');
    while (bin.size() % 4) bin.push_back('\0');
    std::string out;
    auto u32 = [&](uint32_t v) { char b[4]; std::memcpy(b, &v, 4); out.append(b, 4); }; // glb is little-endian; so are our Macs
    const uint32_t total = 12 + 8 + (uint32_t)json.size() + (bin.empty() ? 0 : 8 + (uint32_t)bin.size());
    u32(0x46546C67); // "glTF"
    u32(2);
    u32(total);
    u32((uint32_t)json.size());
    u32(0x4E4F534A); // "JSON"
    out += json;
    if (!bin.empty()) {
        u32((uint32_t)bin.size());
        u32(0x004E4942); // "BIN\0"
        out += bin;
    }
    return writeFile(path, out, error);
}

bool exportGltf(const std::string& path, const Model& m, std::string& error) {
    const std::string binName = stem(path) + ".bin";
    std::string json, bin;
    gltfParts(m, binName, json, bin);
    const size_t slash = path.find_last_of("/\\");
    const std::string dir = slash == std::string::npos ? "" : path.substr(0, slash + 1);
    if (!bin.empty() && !writeFile(dir + binName, bin, error)) return false;
    return writeFile(path, json, error);
}

} // namespace mazz
