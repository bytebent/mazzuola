# Third-party dependencies, fetched at configure time and pinned to exact versions.
# To update one: change the URL, then note it in docs/PLAN.md.
include(FetchContent)
set(FETCHCONTENT_QUIET ON)

# GLFW 3.4 — windows, input, OpenGL context
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND  OFF CACHE BOOL "" FORCE) # X11 on Linux (CI runs under Xvfb)
FetchContent_Declare(glfw
  URL https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

# Dear ImGui (docking branch) — UI, with its own GLFW + OpenGL 3 backends
FetchContent_Declare(imgui
  URL https://github.com/ocornut/imgui/archive/refs/tags/v1.93.0-docking.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

# stb (single-file image libraries; no releases, pinned commit)
FetchContent_Declare(stb
  URL https://github.com/nothings/stb/archive/9600522de53973336fa033a289858a4354cbb04c.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

FetchContent_MakeAvailable(glfw imgui stb)

# One fix to ImGui's GLFW backend, applied to a copy in the build folder: a window it
# creates for a panel pulled out of the main window must never be 0 pixels wide or high
# (GLFW refuses, and the backend then crashed; seen when a Sidecar display went away).
file(READ ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp MAZZ_IMGUI_GLFW)
set(MAZZ_FIND "vd->Window = glfwCreateWindow((int)viewport->Size.x, (int)viewport->Size.y,")
set(MAZZ_REPLACE "vd->Window = glfwCreateWindow((int)viewport->Size.x > 1 ? (int)viewport->Size.x : 1, (int)viewport->Size.y > 1 ? (int)viewport->Size.y : 1,")
string(FIND "${MAZZ_IMGUI_GLFW}" "${MAZZ_FIND}" MAZZ_FOUND)
if(MAZZ_FOUND EQUAL -1)
  message(FATAL_ERROR "imgui_impl_glfw.cpp changed: update the window-size fix in cmake/Deps.cmake")
endif()
string(REPLACE "${MAZZ_FIND}" "${MAZZ_REPLACE}" MAZZ_IMGUI_GLFW "${MAZZ_IMGUI_GLFW}")
file(WRITE ${CMAKE_BINARY_DIR}/generated/imgui_impl_glfw.cpp.tmp "${MAZZ_IMGUI_GLFW}")
file(COPY_FILE ${CMAKE_BINARY_DIR}/generated/imgui_impl_glfw.cpp.tmp
     ${CMAKE_BINARY_DIR}/generated/imgui_impl_glfw.cpp ONLY_IF_DIFFERENT)

# ImGui has no CMake build of its own; build it with its backends as one static lib.
add_library(mazz_imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${CMAKE_BINARY_DIR}/generated/imgui_impl_glfw.cpp
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
