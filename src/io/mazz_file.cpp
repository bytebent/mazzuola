#include "io/mazz_file.hpp"
#include "core/numbers.hpp"
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

namespace mazz {

namespace {

std::string hex(Rgb c) {
    char b[8];
    std::snprintf(b, sizeof b, "%02x%02x%02x", c.r, c.g, c.b);
    return b;
}

bool parseNumber(const std::string& s, double& v) {
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    v = std::strtod(s.c_str(), &end);
    return end && *end == 0 && errno == 0 && std::isfinite(v);
}

bool parseInt(const std::string& s, int& v) {
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    const long l = std::strtol(s.c_str(), &end, 10);
    if (!end || *end != 0 || errno != 0 || l < -2147483647L || l > 2147483647L) return false;
    v = (int)l;
    return true;
}

bool parseHex(const std::string& s, Rgb& c) {
    if (s.size() != 6) return false;
    for (char ch : s)
        if (!std::isxdigit((unsigned char)ch)) return false;
    const unsigned long v = std::strtoul(s.c_str(), nullptr, 16);
    c = {(std::uint8_t)(v >> 16), (std::uint8_t)(v >> 8), (std::uint8_t)v};
    return true;
}

std::vector<std::string> words(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream in(line);
    std::string w;
    while (in >> w) out.push_back(w);
    return out;
}

} // namespace

std::string writeMazz(const MazzModel& m) {
    std::ostringstream o;
    auto n = [](double v) { return formatClean(v); };
    o << "mazzuola 1\n";
    o << "units cm\n";
    o << "texel " << n(m.texel) << '\n';
    for (const Vec3& p : m.mesh.verts) o << "v " << n(p.x) << ' ' << n(p.y) << ' ' << n(p.z) << '\n';
    for (const Face& f : m.mesh.faces) {
        o << 'f';
        for (int k = 0; k < f.n; ++k) o << ' ' << f.v[k];
        if (f.sg) o << " sg=" << f.sg;
        if (f.twoSided) o << " two=1";
        if (f.diag) o << " diag=1";
        o << '\n';
        bool anyUv = false, anyCol = false;
        for (int k = 0; k < f.n; ++k) {
            anyUv = anyUv || !(f.uv[k] == Vec2{});
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
    return o.str();
}

bool readMazz(const std::string& text, MazzModel& out, std::string& error) {
    MazzModel m;
    std::istringstream in(text);
    std::string line;
    int lineNo = 0;
    bool header = false, units = false;
    auto fail = [&](const std::string& why) {
        error = "line " + std::to_string(lineNo) + ": " + why;
        return false;
    };
    while (std::getline(in, line)) {
        ++lineNo;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::vector<std::string> w = words(line);
        if (w.empty() || w[0][0] == '#') continue;
        const std::string& key = w[0];
        if (!header) {
            if (key != "mazzuola") return fail("not a Mazzuola model (it should start with \"mazzuola 1\")");
            if (w.size() != 2 || w[1] != "1") return fail("made by a newer Mazzuola (format " + (w.size() > 1 ? w[1] : "?") + ")");
            header = true;
            continue;
        }
        if (key == "units") {
            if (w.size() != 2 || w[1] != "cm") return fail("units must be cm");
            units = true;
        } else if (key == "texel") {
            double t = 0;
            if (w.size() != 2 || !parseNumber(w[1], t) || t <= 0) return fail("texel needs one positive number");
            m.texel = t;
        } else if (key == "v") {
            Vec3 p;
            if (w.size() != 4 || !parseNumber(w[1], p.x) || !parseNumber(w[2], p.y) || !parseNumber(w[3], p.z))
                return fail("a point needs three numbers");
            m.mesh.verts.push_back(clean(p));
        } else if (key == "f") {
            Face f;
            std::vector<int> idx;
            size_t i = 1;
            for (; i < w.size() && w[i].find('=') == std::string::npos; ++i) {
                int v = 0;
                if (!parseInt(w[i], v)) return fail("bad point number \"" + w[i] + "\"");
                idx.push_back(v);
            }
            if (idx.size() != 3 && idx.size() != 4) return fail("a face needs 3 or 4 points");
            f.n = (int)idx.size();
            for (int k = 0; k < f.n; ++k) {
                if (idx[k] < 0 || idx[k] >= (int)m.mesh.verts.size())
                    return fail("point " + std::to_string(idx[k]) + " doesn't exist (points come before faces)");
                for (int j = 0; j < k; ++j)
                    if (idx[j] == idx[k]) return fail("a face uses the same point twice");
                f.v[k] = idx[k];
            }
            for (; i < w.size(); ++i) {
                const size_t eq = w[i].find('=');
                if (eq == std::string::npos) return fail("expected name=value, got \"" + w[i] + "\"");
                const std::string name = w[i].substr(0, eq), value = w[i].substr(eq + 1);
                int v = 0;
                if (!parseInt(value, v)) return fail("bad value for " + name);
                if (name == "sg" && v >= 0) f.sg = v;
                else if (name == "two" && (v == 0 || v == 1)) f.twoSided = v == 1;
                else if (name == "diag" && (v == 0 || v == 1)) f.diag = v == 1;
                else return fail("unknown face setting \"" + w[i] + "\"");
            }
            m.mesh.faces.push_back(f);
        } else if (key == "uv" || key == "col") {
            if (m.mesh.faces.empty()) return fail(key + " must follow a face");
            Face& f = m.mesh.faces.back();
            if (key == "uv") {
                // "uv u v / u v / ..." : the slashes are only for reading.
                std::vector<double> nums;
                for (size_t i = 1; i < w.size(); ++i) {
                    if (w[i] == "/") continue;
                    double d = 0;
                    if (!parseNumber(w[i], d)) return fail("bad texture coordinate \"" + w[i] + "\"");
                    nums.push_back(d);
                }
                if ((int)nums.size() != 2 * f.n) return fail("uv needs two numbers per corner");
                for (int k = 0; k < f.n; ++k) f.uv[k] = {clean(nums[2 * k]), clean(nums[2 * k + 1])};
            } else {
                if ((int)w.size() != 1 + f.n) return fail("col needs one colour per corner");
                for (int k = 0; k < f.n; ++k)
                    if (!parseHex(w[1 + k], f.col[k])) return fail("bad colour \"" + w[1 + k] + "\" (use rrggbb)");
            }
        } else {
            return fail("unknown line \"" + key + "\"");
        }
    }
    if (!header) {
        lineNo = 1;
        return fail("empty file");
    }
    if (!units) {
        lineNo = 2;
        return fail("missing \"units cm\"");
    }
    out = std::move(m);
    return true;
}

bool saveMazzFile(const std::string& path, const MazzModel& m, std::string& error) {
    const std::string tmp = path + ".saving";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) {
            error = "can't write there";
            return false;
        }
        const std::string text = writeMazz(m);
        f.write(text.data(), (std::streamsize)text.size());
        f.flush();
        if (!f) {
            error = "writing failed (disk full?)";
            std::remove(tmp.c_str());
            return false;
        }
    }
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        error = std::string("can't replace the file: ") + std::strerror(errno);
        std::remove(tmp.c_str());
        return false;
    }
    return true;
}

bool loadMazzFile(const std::string& path, MazzModel& out, std::string& error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        error = "can't open the file";
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return readMazz(ss.str(), out, error);
}

} // namespace mazz
