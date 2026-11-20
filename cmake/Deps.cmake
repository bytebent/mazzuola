# Third-party dependencies, fetched at configure time and pinned to exact versions.
# Each download is checked against its SHA-256: a changed or swapped archive stops the
# build. To update one: change the URL, the hash and the commit, then note it in the LOG.
include(FetchContent)
set(FETCHCONTENT_QUIET ON)

# GLFW 3.4 (commit 7b6aead9fb88b3623e3b3725ebb42670cbe4c579) — windows, input, OpenGL context
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND  OFF CACHE BOOL "" FORCE) # X11 on Linux (CI runs under Xvfb)
FetchContent_Declare(glfw
  URL https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz
  URL_HASH SHA256=c038d34200234d071fae9345bc455e4a8f2f544ab60150765d7704e08f3dac01
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

# Dear ImGui v1.93.0-docking (commit b916186e66aa3c02b72c6ca17455516512ed8049) — UI, with its own GLFW + OpenGL 3 backends
FetchContent_Declare(imgui
  URL https://github.com/ocornut/imgui/archive/refs/tags/v1.93.0-docking.tar.gz
  URL_HASH SHA256=746c265540bd3aa109570f29492ef4d8f5b421dd9fd0049c35ca35f21496f1a5
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

# stb (single-file image libraries; no releases, pinned commit)
FetchContent_Declare(stb
  URL https://github.com/nothings/stb/archive/9600522de53973336fa033a289858a4354cbb04c.tar.gz
  URL_HASH SHA256=8546b1281cf61e4b5229e9eb05055d765f1b076f6ba03aab377df09a20fb613b
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

FetchContent_MakeAvailable(glfw imgui stb)

# ImGui has no CMake build of its own; build it with its backends as one static lib.
add_library(mazz_imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(mazz_imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(mazz_imgui PUBLIC glfw)
# The item hooks (meant for test engines) let UI scripts catch duplicate widget IDs
# (src/ui/imgui_checks.cpp). They only run when switched on, in scripted test runs.
target_compile_definitions(mazz_imgui PUBLIC IMGUI_ENABLE_TEST_ENGINE)
if(APPLE)
  target_compile_definitions(mazz_imgui PRIVATE GL_SILENCE_DEPRECATION)
endif()

add_library(mazz_stb INTERFACE)
target_include_directories(mazz_stb INTERFACE ${stb_SOURCE_DIR})

# UI font: Roboto Medium (Apache License 2.0), shipped with Dear ImGui. Embedded into the
# app so it needs no files at runtime. See docs/THIRD_PARTY.md.
include(${CMAKE_CURRENT_LIST_DIR}/EmbedFile.cmake)
embed_file(${imgui_SOURCE_DIR}/misc/fonts/Roboto-Medium.ttf
           ${CMAKE_BINARY_DIR}/generated/font_roboto.cpp mazz_font_roboto)
add_library(mazz_assets STATIC ${CMAKE_BINARY_DIR}/generated/font_roboto.cpp)
