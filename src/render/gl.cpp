#include "render/gl.hpp"

#if !defined(__APPLE__)
#define MAZZ_GL_DEFINE(type, name) type mazz_##name = nullptr;
MAZZ_GL_FUNCTIONS(MAZZ_GL_DEFINE)
#undef MAZZ_GL_DEFINE
#endif

namespace mazz {

bool loadGL(GLProcLoader loader, const char** missing) {
#if defined(__APPLE__)
    (void)loader;
    (void)missing;
    return true;
#else
#define MAZZ_GL_LOAD(type, name)                                   \
    mazz_##name = reinterpret_cast<type>(loader(#name));           \
    if (!mazz_##name) { if (missing) *missing = #name; return false; }
    MAZZ_GL_FUNCTIONS(MAZZ_GL_LOAD)
#undef MAZZ_GL_LOAD
    return true;
#endif
}

} // namespace mazz
