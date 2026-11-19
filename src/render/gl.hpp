#pragma once
// OpenGL access. macOS: the system framework (functions linked directly).
// Elsewhere: function pointers loaded at startup through a small list (only the
// functions Mazzuola uses), so no loader library is needed.
#if defined(__APPLE__)
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif
#include <OpenGL/gl3.h>
#else
#include <GL/glcorearb.h>

#define MAZZ_GL_FUNCTIONS(X)                                                    \
    X(PFNGLCLEARPROC, glClear)                                                  \
    X(PFNGLCLEARCOLORPROC, glClearColor)                                        \
    X(PFNGLVIEWPORTPROC, glViewport)                                            \
    X(PFNGLENABLEPROC, glEnable)                                                \
    X(PFNGLDISABLEPROC, glDisable)                                              \
    X(PFNGLBLENDFUNCSEPARATEPROC, glBlendFuncSeparate)                          \
    X(PFNGLDEPTHFUNCPROC, glDepthFunc)                                          \
    X(PFNGLDEPTHMASKPROC, glDepthMask)                                          \
    X(PFNGLGETINTEGERVPROC, glGetIntegerv)                                      \
    X(PFNGLGETERRORPROC, glGetError)                                            \
    X(PFNGLREADPIXELSPROC, glReadPixels)                                        \
    X(PFNGLPIXELSTOREIPROC, glPixelStorei)                                      \
    X(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers)                              \
    X(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer)                              \
    X(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D)                    \
    X(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus)                \
    X(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers)                        \
    X(PFNGLGENRENDERBUFFERSPROC, glGenRenderbuffers)                            \
    X(PFNGLBINDRENDERBUFFERPROC, glBindRenderbuffer)                            \
    X(PFNGLRENDERBUFFERSTORAGEPROC, glRenderbufferStorage)                      \
    X(PFNGLFRAMEBUFFERRENDERBUFFERPROC, glFramebufferRenderbuffer)              \
    X(PFNGLDELETERENDERBUFFERSPROC, glDeleteRenderbuffers)                      \
    X(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC, glRenderbufferStorageMultisample) \
    X(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer)                              \
    X(PFNGLGENTEXTURESPROC, glGenTextures)                                      \
    X(PFNGLBINDTEXTUREPROC, glBindTexture)                                      \
    X(PFNGLTEXIMAGE2DPROC, glTexImage2D)                                        \
    X(PFNGLTEXPARAMETERIPROC, glTexParameteri)                                  \
    X(PFNGLDELETETEXTURESPROC, glDeleteTextures)                                \
    X(PFNGLCREATESHADERPROC, glCreateShader)                                    \
    X(PFNGLSHADERSOURCEPROC, glShaderSource)                                    \
    X(PFNGLCOMPILESHADERPROC, glCompileShader)                                  \
    X(PFNGLGETSHADERIVPROC, glGetShaderiv)                                      \
    X(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog)                            \
    X(PFNGLDELETESHADERPROC, glDeleteShader)                                    \
    X(PFNGLCREATEPROGRAMPROC, glCreateProgram)                                  \
    X(PFNGLATTACHSHADERPROC, glAttachShader)                                    \
    X(PFNGLLINKPROGRAMPROC, glLinkProgram)                                      \
    X(PFNGLGETPROGRAMIVPROC, glGetProgramiv)                                    \
    X(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog)                          \
    X(PFNGLDELETEPROGRAMPROC, glDeleteProgram)                                  \
    X(PFNGLUSEPROGRAMPROC, glUseProgram)                                        \
    X(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation)                        \
    X(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv)                            \
    X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays)                              \
    X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray)                              \
    X(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays)                        \
    X(PFNGLGENBUFFERSPROC, glGenBuffers)                                        \
    X(PFNGLBINDBUFFERPROC, glBindBuffer)                                        \
    X(PFNGLBUFFERDATAPROC, glBufferData)                                        \
    X(PFNGLDELETEBUFFERSPROC, glDeleteBuffers)                                  \
    X(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer)                      \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray)              \
    X(PFNGLDRAWARRAYSPROC, glDrawArrays)                                        \
    X(PFNGLDRAWELEMENTSPROC, glDrawElements)                                    \
    X(PFNGLPOLYGONOFFSETPROC, glPolygonOffset)                                  \
    X(PFNGLUNIFORM4FPROC, glUniform4f)                                          \
    X(PFNGLUNIFORM3FPROC, glUniform3f)                                          \
    X(PFNGLUNIFORM1FPROC, glUniform1f)

#define MAZZ_GL_DECLARE(type, name) extern type mazz_##name;
MAZZ_GL_FUNCTIONS(MAZZ_GL_DECLARE)
#undef MAZZ_GL_DECLARE

#define glClear mazz_glClear
#define glClearColor mazz_glClearColor
#define glViewport mazz_glViewport
#define glEnable mazz_glEnable
#define glDisable mazz_glDisable
#define glBlendFuncSeparate mazz_glBlendFuncSeparate
#define glDepthFunc mazz_glDepthFunc
#define glDepthMask mazz_glDepthMask
#define glGetIntegerv mazz_glGetIntegerv
#define glGetError mazz_glGetError
#define glReadPixels mazz_glReadPixels
#define glPixelStorei mazz_glPixelStorei
#define glGenFramebuffers mazz_glGenFramebuffers
#define glBindFramebuffer mazz_glBindFramebuffer
#define glFramebufferTexture2D mazz_glFramebufferTexture2D
#define glCheckFramebufferStatus mazz_glCheckFramebufferStatus
#define glDeleteFramebuffers mazz_glDeleteFramebuffers
#define glGenRenderbuffers mazz_glGenRenderbuffers
#define glBindRenderbuffer mazz_glBindRenderbuffer
#define glRenderbufferStorage mazz_glRenderbufferStorage
#define glFramebufferRenderbuffer mazz_glFramebufferRenderbuffer
#define glDeleteRenderbuffers mazz_glDeleteRenderbuffers
#define glRenderbufferStorageMultisample mazz_glRenderbufferStorageMultisample
#define glBlitFramebuffer mazz_glBlitFramebuffer
#define glGenTextures mazz_glGenTextures
#define glBindTexture mazz_glBindTexture
#define glTexImage2D mazz_glTexImage2D
#define glTexParameteri mazz_glTexParameteri
#define glDeleteTextures mazz_glDeleteTextures
#define glCreateShader mazz_glCreateShader
#define glShaderSource mazz_glShaderSource
#define glCompileShader mazz_glCompileShader
#define glGetShaderiv mazz_glGetShaderiv
#define glGetShaderInfoLog mazz_glGetShaderInfoLog
#define glDeleteShader mazz_glDeleteShader
#define glCreateProgram mazz_glCreateProgram
#define glAttachShader mazz_glAttachShader
#define glLinkProgram mazz_glLinkProgram
#define glGetProgramiv mazz_glGetProgramiv
#define glGetProgramInfoLog mazz_glGetProgramInfoLog
#define glDeleteProgram mazz_glDeleteProgram
#define glUseProgram mazz_glUseProgram
#define glGetUniformLocation mazz_glGetUniformLocation
#define glUniformMatrix4fv mazz_glUniformMatrix4fv
#define glGenVertexArrays mazz_glGenVertexArrays
#define glBindVertexArray mazz_glBindVertexArray
#define glDeleteVertexArrays mazz_glDeleteVertexArrays
#define glGenBuffers mazz_glGenBuffers
#define glBindBuffer mazz_glBindBuffer
#define glBufferData mazz_glBufferData
#define glDeleteBuffers mazz_glDeleteBuffers
#define glVertexAttribPointer mazz_glVertexAttribPointer
#define glEnableVertexAttribArray mazz_glEnableVertexAttribArray
#define glDrawArrays mazz_glDrawArrays
#define glDrawElements mazz_glDrawElements
#define glPolygonOffset mazz_glPolygonOffset
#define glUniform4f mazz_glUniform4f
#define glUniform3f mazz_glUniform3f
#define glUniform1f mazz_glUniform1f
#endif

namespace mazz {

using GLProcLoader = void* (*)(const char* name);

// Loads the function pointers (no-op on macOS). Returns false and names the first
// missing function in `missing` if any is unavailable.
bool loadGL(GLProcLoader loader, const char** missing);

} // namespace mazz
