#pragma once
// Exports for other programs: glTF (.glb, or .gltf with a .bin beside it) and Wavefront
// .obj. Both are converted the way those formats expect: Y up and metres (Mazzuola is Z up
// and centimetres), with the model's pivot as the origin. The original units and pivot go
// into glTF "extras", so Scalpellino can convert back exactly.
// Milestone 1 writes geometry and normals (from smoothing groups); textures and vertex
// colours join in milestones 5 and 7.
#include "core/document.hpp"
#include <string>

namespace mazz {

// Mazzuola (Z up, cm, relative to the pivot) -> export space (Y up, m).
Vec3 toExportSpace(const Vec3& p, const Vec3& pivot);
Vec3 toExportDirection(const Vec3& d);

bool exportGlb(const std::string& path, const Model& m, std::string& error);
bool exportGltf(const std::string& path, const Model& m, std::string& error); // also writes <name>.bin
bool exportObj(const std::string& path, const Model& m, std::string& error);

// In-memory forms (for tests).
std::string objText(const Model& m);
// JSON and binary buffer of a glTF; `binUri` empty = for a .glb (buffer without uri).
void gltfParts(const Model& m, const std::string& binUri, std::string& json, std::string& bin);

} // namespace mazz
