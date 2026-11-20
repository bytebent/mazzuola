#pragma once
// Export (ARCHITECTURE §7, "Alpha 1 export"): .glb, .gltf (+ .bin) and .obj (+ .mtl).
// Mazzuola is Z-up in centimetres; every export is Y-up in metres: (x, y, z) cm becomes
// (x, z, -y) / 100 m. The world origin stays the zero point. Every edge is hard (flat
// normals) until smoothing groups arrive; quads are split along their stored diagonal,
// as drawn; one plain grey material. Pure C++ (no graphics).
#include "core/mesh.hpp"
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace mazz {

enum class ExportFormat { Glb, Gltf, Obj };

const char* exportExtension(ExportFormat f); // "glb", "gltf", "obj"
const char* exportFormatName(ExportFormat f); // "glTF binary (.glb)" ...
// From the file's extension (case ignored); false if it is none of the three.
bool exportFormatFromPath(const std::string& path, ExportFormat& out);

// The model as flat-shaded triangles, already Y-up metres: three corners per triangle
// side by side would waste space, so each face's corners are shared by its triangles
// (a quad is 4 vertices, 6 indices), each with the face's normal.
struct ExportMesh {
    std::vector<float> pos;      // x y z per vertex (metres, Y-up)
    std::vector<float> nrm;      // x y z per vertex
    std::vector<std::uint32_t> idx;
    float mn[3] = {0, 0, 0}, mx[3] = {0, 0, 0};
    int vertexCount() const { return (int)(pos.size() / 3); }
};
ExportMesh buildExportMesh(const Mesh& m);

// Writes the model. `generator` goes into the glTF asset ("Mazzuola 0.1.4"). Files are
// written next to the target first and renamed, so a failed export never leaves a
// half-written file, and a .gltf is never left paired with another export's .bin. Returns false with `error` set on failure (or an empty model).
bool exportModel(const std::string& path, ExportFormat f, const Mesh& m, const std::string& generator,
                 std::string& error);

// The .obj and .mtl text (exposed for tests). `mtlName` is the .mtl's file name.
std::string writeObj(const Mesh& m, const std::string& objectName, const std::string& mtlName,
                     const std::string& generator);
std::string writeMtl();

// Renames each (temporary, final) pair into place, all or nothing: on failure the old
// files are back and the temporaries removed (exposed for tests).
bool commitExportFiles(const std::vector<std::pair<std::string, std::string>>& files,
                       std::string& error);

} // namespace mazz
