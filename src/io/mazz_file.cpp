#include "io/mazz_file.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace mazz {

std::string formatNumber(double v) {
    if (v == 0) return "0"; // also turns -0 into 0
    char buf[64];
    for (int prec = 1; prec <= 17; ++prec) {
        std::snprintf(buf, sizeof buf, "%.*g", prec, v);
        if (std::strtod(buf, nullptr) == v) break;
    }
    return buf;
}

namespace {
std::string hex(Rgb c) {
    char b[8];
    std::snprintf(b, sizeof b, "%02x%02x%02x", c.r, c.g, c.b);
    return b;
}

std::string quote(const std::string& s) {
    std::string q = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') q += '\\';
        q += c;
    }
    return q + "\"";
}
} // namespace

std::string writeMazz(const Model& m) {
    std::ostringstream o;
    auto n = [](double v) { return formatNumber(v); };
    o << "mazzuola 1\n";
    o << "units cm\n";
    o << "pivot " << n(m.pivot.x) << ' ' << n(m.pivot.y) << ' ' << n(m.pivot.z) << '\n';
    o << "texel " << n(m.texel) << '\n';
    for (const Vec3& v : m.mesh.verts) o << "v " << n(v.x) << ' ' << n(v.y) << ' ' << n(v.z) << '\n';
    for (const Face& f : m.mesh.faces) {
        o << 'f';
        for (int k = 0; k < f.n; ++k) o << ' ' << f.v[k];
        if (f.sg) o << " sg=" << f.sg;
        if (f.tex >= 0) o << " tex=" << f.tex;
        if (f.part >= 0) o << " part=" << f.part;
        if (f.twoSided) o << " two=1";
        if (f.diag) o << " diag=1";
        if (f.hidden) o << " hidden=1";
        o << '\n';
        bool anyUv = false, anyCol = false;
        for (int k = 0; k < f.n; ++k) {
            anyUv = anyUv || f.uv[k].x != 0 || f.uv[k].y != 0;
            anyCol = anyCol || !(f.col[k] == Rgb{});
        }
        if (anyUv) {
            o << "  uv";
            for (int k = 0; k < f.n; ++k) o << (k ? " / " : " ") << n(f.uv[k].x) << ' ' << n(f.uv[k].y);
            o << '\n';
        }
        if (anyCol) {
            o << "  col";
            for (int k = 0; k < f.n; ++k) o << ' ' << hex(f.col[k]);
            o << '\n';
        }
    }
    for (const RefImage& r : m.refs) {
        o << "ref " << viewDirName(r.dir) << ' ' << quote(r.path) << " x=" << n(r.x) << " y=" << n(r.y)
          << " rot=" << n(r.rotation) << " width=" << n(r.width) << " alpha=" << n(r.alpha)
          << " layer=" << (r.inFront ? "front" : "back") << '\n';
    }
    return o.str();
}

namespace {
// Splits a line into words; "quoted text" is one word (without the quotes).
bool tokenize(const std::string& line, std::vector<std::string>& out) {
    out.clear();
    size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r')) ++i;
        if (i >= line.size()) break;
        std::string w;
        if (line[i] == '"') {
            ++i;
            bool closed = false;
            while (i < line.size()) {
                if (line[i] == '\\' && i + 1 < line.size()) { w += line[i + 1]; i += 2; continue; }
                if (line[i] == '"') { closed = true; ++i; break; }
                w += line[i++];
            }
            if (!closed) return false;
        } else {
            while (i < line.size() && line[i] != ' ' && line[i] != '\t' && line[i] != '\r') w += line[i++];
        }
        out.push_back(w);
    }
    return true;
}

bool num(const std::string& s, double& v) {
    if (s.empty()) return false;
    char* end = nullptr;
    v = std::strtod(s.c_str(), &end);
    return end && *end == 0 && std::isfinite(v);
}
bool integer(const std::string& s, int& v) {
    if (s.empty()) return false;
    char* end = nullptr;
    const long l = std::strtol(s.c_str(), &end, 10);
    if (!end || *end != 0) return false;
    v = (int)l;
    return true;
}
bool hexColour(const std::string& s, Rgb& c) {
    if (s.size() != 6) return false;
    char* end = nullptr;
    const unsigned long v = std::strtoul(s.c_str(), &end, 16);
    if (!end || *end != 0) return false;
    c = {(uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v};
    return true;
}
} // namespace

bool readMazz(const std::string& text, Model& out, std::string& error) {
    Model m;
    std::istringstream in(text);
    std::string line;
    int lineNo = 0;
    bool header = false;
    std::vector<std::string> w;
    auto fail = [&](const std::string& msg) {
        error = "line " + std::to_string(lineNo) + ": " + msg;
        return false;
    };
    while (std::getline(in, line)) {
        ++lineNo;
        if (!tokenize(line, w)) return fail("unclosed quote");
        if (w.empty() || w[0][0] == '#') continue;
        const std::string& k = w[0];
        if (!header) {
            if (k != "mazzuola" || w.size() < 2) return fail("not a Mazzuola file");
            int ver = 0;
            if (!integer(w[1], ver) || ver != 1) return fail("unsupported version " + w[1]);
            header = true;
            continue;
        }
        if (k == "units") {
            if (w.size() != 2 || w[1] != "cm") return fail("unsupported units");
        } else if (k == "pivot") {
            if (w.size() != 4 || !num(w[1], m.pivot.x) || !num(w[2], m.pivot.y) || !num(w[3], m.pivot.z))
                return fail("bad pivot");
        } else if (k == "texel") {
            if (w.size() != 2 || !num(w[1], m.texel)) return fail("bad texel");
        } else if (k == "v") {
            Vec3 p;
            if (w.size() != 4 || !num(w[1], p.x) || !num(w[2], p.y) || !num(w[3], p.z)) return fail("bad point");
            m.mesh.verts.push_back(p);
        } else if (k == "f") {
            Face f;
            int corners = 0;
            size_t i = 1;
            for (; i < w.size() && w[i].find('=') == std::string::npos; ++i) {
                if (corners >= 4) return fail("a face has at most 4 corners");
                if (!integer(w[i], f.v[corners])) return fail("bad corner");
                if (f.v[corners] < 0 || f.v[corners] >= (int)m.mesh.verts.size()) return fail("corner refers to a missing point");
                ++corners;
            }
            if (corners < 3) return fail("a face needs 3 or 4 corners");
            f.n = corners;
            for (; i < w.size(); ++i) {
                const size_t eq = w[i].find('=');
                if (eq == std::string::npos) return fail("expected key=value");
                const std::string key = w[i].substr(0, eq), val = w[i].substr(eq + 1);
                int iv = 0;
                if (!integer(val, iv)) return fail("bad value for " + key);
                if (key == "sg") f.sg = iv;
                else if (key == "tex") f.tex = iv;
                else if (key == "part") f.part = iv;
                else if (key == "two") f.twoSided = iv != 0;
                else if (key == "diag") f.diag = iv != 0;
                else if (key == "hidden") f.hidden = iv != 0;
                else return fail("unknown face setting " + key);
            }
            m.mesh.faces.push_back(f);
        } else if (k == "uv" || k == "col") {
            if (m.mesh.faces.empty()) return fail(k + " before any face");
            Face& f = m.mesh.faces.back();
            if (k == "uv") {
                std::vector<double> vals;
                for (size_t i = 1; i < w.size(); ++i) {
                    if (w[i] == "/") continue;
                    double v;
                    if (!num(w[i], v)) return fail("bad texture coordinate");
                    vals.push_back(v);
                }
                if ((int)vals.size() != f.n * 2) return fail("uv needs one pair per corner");
                for (int c = 0; c < f.n; ++c) f.uv[c] = {vals[c * 2], vals[c * 2 + 1]};
            } else {
                if ((int)w.size() != f.n + 1) return fail("col needs one colour per corner");
                for (int c = 0; c < f.n; ++c)
                    if (!hexColour(w[c + 1], f.col[c])) return fail("bad colour " + w[c + 1]);
            }
        } else if (k == "ref") {
            if (w.size() < 3) return fail("bad reference image");
            RefImage r;
            if (!viewDirFromName(w[1].c_str(), r.dir) || r.dir == ViewDir::Persp) return fail("bad view direction " + w[1]);
            r.path = w[2];
            for (size_t i = 3; i < w.size(); ++i) {
                const size_t eq = w[i].find('=');
                if (eq == std::string::npos) return fail("expected key=value");
                const std::string key = w[i].substr(0, eq), val = w[i].substr(eq + 1);
                if (key == "layer") {
                    if (val != "front" && val != "back") return fail("layer is front or back");
                    r.inFront = val == "front";
                    continue;
                }
                double v;
                if (!num(val, v)) return fail("bad value for " + key);
                if (key == "x") r.x = v;
                else if (key == "y") r.y = v;
                else if (key == "rot") r.rotation = v;
                else if (key == "width") r.width = v;
                else if (key == "alpha") r.alpha = v;
                else return fail("unknown reference setting " + key);
            }
            m.refs.push_back(r);
        } else {
            return fail("unknown line '" + k + "'");
        }
    }
    if (!header) {
        lineNo = 0;
        return fail("empty file");
    }
    out = std::move(m);
    return true;
}

bool saveMazzFile(const std::string& path, const Model& m, std::string& error) {
    // Write to a temporary file first, then rename: a failed save never leaves half a file.
    const std::string tmp = path + ".saving";
    {
        std::ofstream f(tmp, std::ios::binary);
        if (!f) { error = "can't write " + path; return false; }
        f << writeMazz(m);
        if (!f) { error = "can't write " + path; return false; }
    }
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        std::remove(tmp.c_str());
        error = "can't write " + path;
        return false;
    }
    return true;
}

bool loadMazzFile(const std::string& path, Model& out, std::string& error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { error = "can't open " + path; return false; }
    std::stringstream ss;
    ss << f.rdbuf();
    return readMazz(ss.str(), out, error);
}

} // namespace mazz
