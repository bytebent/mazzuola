#pragma once
// Mazzuola's own file format (.mazz): plain text, one model per file, readable and
// diffable. Writing then reading gives back exactly the same model.
//
//   mazzuola 1
//   units cm
//   pivot 0 0 0
//   texel 0.5
//   v 0 0 0                                  one line per point
//   f 0 1 2 3 sg=1 two=1 diag=1 hidden=1 tex=0 part=0   one per face (defaults left out)
//     uv 0 0 / 16 0 / 16 16 / 0 16           only if not all zero
//     col ffffff ffffff c0c0c0 c0c0c0        only if not all white
//   ref Front "front.png" x=0 y=0 rot=0 width=100 alpha=0.5 layer=back
#include "core/document.hpp"
#include <string>

namespace mazz {

std::string writeMazz(const Model& m);
// Reads a model; on failure returns false with a message naming the line.
bool readMazz(const std::string& text, Model& out, std::string& error);

bool saveMazzFile(const std::string& path, const Model& m, std::string& error);
bool loadMazzFile(const std::string& path, Model& out, std::string& error);

// Shortest decimal text that reads back as exactly the same double ("0.1", "12", "-3.5").
std::string formatNumber(double v);

} // namespace mazz
