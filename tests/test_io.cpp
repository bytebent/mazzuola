// Tests for the .mazz file: exact text for a known model, round trips, clear errors.
#include "check.hpp"
#include "core/numbers.hpp"
#include "io/mazz_file.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

using namespace mazz;

namespace {

bool sameMesh(const Mesh& a, const Mesh& b) {
    if (a.verts.size() != b.verts.size() || a.faces.size() != b.faces.size()) return false;
    for (size_t i = 0; i < a.verts.size(); ++i)
        if (a.verts[i].x != b.verts[i].x || a.verts[i].y != b.verts[i].y || a.verts[i].z != b.verts[i].z) return false;
    for (size_t i = 0; i < a.faces.size(); ++i)
        if (a.faces[i] != b.faces[i]) return false;
    return true;
}

bool errorAt(const std::string& text, int line) {
    MazzModel m;
    std::string err;
    if (readMazz(text, m, err)) return false;
    return err.rfind("line " + std::to_string(line) + ":", 0) == 0;
}

void testGolden() {
    MazzModel m;
    addBox(m.mesh, {0, 0, 0}, {100, 50, 62.499999999999986});
    m.mesh.verts.push_back({3.061616997868383e-15, -0.125, 1000}); // a stray point
    m.mesh.faces[1].sg = 2;
    m.mesh.faces[2].twoSided = true;
    m.mesh.faces[3].uv[1] = {16, 0};
    m.mesh.faces[4].col[2] = {0xc0, 0x80, 0x00};
    const std::string expected =
        "mazzuola 1\n"
        "units cm\n"
        "texel 0.5\n"
        "v 0 0 0\n"
        "v 100 0 0\n"
        "v 0 50 0\n"
        "v 100 50 0\n"
        "v 0 0 62.5\n"
        "v 100 0 62.5\n"
        "v 0 50 62.5\n"
        "v 100 50 62.5\n"
        "v 0 -0.125 1000\n"
        "f 0 2 3 1\n"
        "f 4 5 7 6 sg=2\n"
        "f 0 1 5 4 two=1\n"
        "f 2 6 7 3\n"
        "  uv 0 0 / 16 0 / 0 0 / 0 0\n"
        "f 0 4 6 2\n"
        "  col ffffff ffffff c08000 ffffff\n"
        "f 1 3 7 5\n";
    const std::string text = writeMazz(m);
    CHECK(text == expected);
    if (text != expected) std::fprintf(stderr, "--- written:\n%s--- expected:\n%s", text.c_str(), expected.c_str());

    MazzModel back;
    std::string err;
    CHECK(readMazz(text, back, err));
    CHECK(writeMazz(back) == text);
}

void testRoundTrip() {
    std::srand(11);
    MazzModel m;
    for (int i = 0; i < 300; ++i)
        m.mesh.verts.push_back(clean(Vec3{(std::rand() % 200001 - 100000) / 997.0, (std::rand() % 2001) / 3.0,
                                          -(std::rand() % 5000) / 7.0}));
    for (int i = 0; i + 3 < 300; i += 4) {
        Face f;
        f.n = (i % 8) ? 4 : 3;
        for (int k = 0; k < f.n; ++k) {
            f.v[k] = i + k;
            f.uv[k] = {clean(i * 0.37), clean(k * 1.5)};
            f.col[k] = {(std::uint8_t)i, (std::uint8_t)(k * 50), 7};
        }
        f.sg = i % 3;
        f.diag = (i % 5) == 0;
        m.mesh.faces.push_back(f);
    }
    MazzModel back;
    std::string err;
    CHECK(readMazz(writeMazz(m), back, err));
    CHECK(sameMesh(m.mesh, back.mesh));

    // Through a real file too (written via a temporary file and a rename).
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path dir = fs::temp_directory_path(ec) / ("mazzio-" + std::to_string(std::rand()));
    if (fs::create_directories(dir, ec)) {
        const std::string path = (dir / "model.mazz").string();
        CHECK(saveMazzFile(path, m, err));
        MazzModel f;
        CHECK(loadMazzFile(path, f, err));
        CHECK(sameMesh(m.mesh, f.mesh));
        CHECK(!fs::exists(path + ".saving")); // no leftover
        fs::remove_all(dir, ec);
    } else {
        CHECK(false); // no temp folder
    }
    CHECK(!loadMazzFile("/nonexistent/x.mazz", back, err) && !err.empty());
}

void testErrors() {
    const std::string head = "mazzuola 1\nunits cm\n";
    CHECK(errorAt("", 1));
    CHECK(errorAt("hello\n", 1));
    CHECK(errorAt("mazzuola 2\n", 1));
    CHECK(errorAt("mazzuola 1\nv 0 0 0\n", 2)); // no units line
    CHECK(errorAt(head + "v 0 0\n", 3));
    CHECK(errorAt(head + "v 0 0 x\n", 3));
    CHECK(errorAt(head + "v 0 0 0\nv 1 0 0\nf 0 1\n", 5));
    CHECK(errorAt(head + "v 0 0 0\nv 1 0 0\nv 1 1 0\nf 0 1 3\n", 6));
    CHECK(errorAt(head + "v 0 0 0\nv 1 0 0\nv 1 1 0\nf 0 1 1\n", 6));
    CHECK(errorAt(head + "v 0 0 0\nv 1 0 0\nv 1 1 0\nf 0 1 2 wobble=1\n", 6));
    CHECK(errorAt(head + "v 0 0 0\nv 1 0 0\nv 1 1 0\nf 0 1 2\n  uv 0 0 / 1 0\n", 7));
    CHECK(errorAt(head + "v 0 0 0\nv 1 0 0\nv 1 1 0\nf 0 1 2\n  col ffffff zzzzzz ffffff\n", 7));
    CHECK(errorAt(head + "uv 0 0\n", 3));
    CHECK(errorAt(head + "teapot\n", 3));

    // Comments, blank lines and Windows line ends are fine; noisy numbers are cleaned.
    MazzModel m;
    std::string err;
    CHECK(readMazz("# a comment\r\nmazzuola 1\r\n\r\nunits cm\r\nv 3.061616997868383e-15 21.650635094610966 1e+02\r\n",
                   m, err));
    CHECK(m.mesh.verts.size() == 1 && m.mesh.verts[0].x == 0 && m.mesh.verts[0].y == 21.651 && m.mesh.verts[0].z == 100);
}

} // namespace

void testIo() {
    testGolden();
    testRoundTrip();
    testErrors();
}
