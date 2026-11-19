#pragma once
// The native .mazz file (ARCHITECTURE §7): plain text, one line per point and face,
// numbers as plain decimals (core/numbers.hpp), so files are readable and diffable and
// round-trip exactly. Pure C++ (no graphics).
//
//   mazzuola 1
//   units cm
//   texel 0.5
//   v 0 0 0
//   f 0 1 2 3 sg=1 two=1 diag=1      (attributes only when not the default)
//     uv 0 0 / 16 0 / 16 16 / 0 16   (only when not all 0)
//     col ffffff ffffff c0c0c0 c0c0c0 (only when not all white)
//
// Textures, groups and palettes join the format in later milestones.
#include "core/mesh.hpp"
#include <string>

namespace mazz {

struct MazzModel {
    Mesh mesh;
    double texel = 0.5; // texture pixels per cm (milestone 5)
};

std::string writeMazz(const MazzModel& m);
// Fills `out`; on failure returns false and `error` says which line and why.
bool readMazz(const std::string& text, MazzModel& out, std::string& error);

// Whole-file helpers. Saving writes a temporary file next to the target and renames it,
// so a failed save never leaves a half-written model behind.
bool saveMazzFile(const std::string& path, const MazzModel& m, std::string& error);
bool loadMazzFile(const std::string& path, MazzModel& out, std::string& error);

} // namespace mazz
