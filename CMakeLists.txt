cmake_minimum_required(VERSION 3.24)
project(Mazzuola VERSION 0.2.1 LANGUAGES C CXX)
# Objective-C++ (native macOS file dialogs, tablet input later) and Objective-C (GLFW's
# Cocoa files; without OBJC enabled, CMake would compile those .m files as Objective-C++,
# which fails). Must be enabled here, at the top level, before any dependency is added.
if(APPLE)
  enable_language(OBJC OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MAZZ_BUILD_APP   "Build the app (needs GLFW/ImGui)" ON)
option(MAZZ_BUILD_TESTS "Build core tests" ON)

# Generic minimum macOS version (not tied to any particular machine).
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------- core
# Pure C++: no OpenGL, GLFW or ImGui. Must stay that way (see CLAUDE.md).
add_library(mazz_core STATIC
  src/core/camera.cpp
  src/core/grid_lines.cpp
  src/core/numbers.cpp
  src/core/mesh.cpp
  src/core/selection.cpp
  src/core/document.cpp
  src/core/view_geom.cpp
  src/core/draw_data.cpp
  src/core/transform.cpp
  src/core/pick.cpp
  src/core/primitives.cpp
  src/core/rock.cpp
  src/core/select_ops.cpp
  src/core/ops.cpp
  src/core/figure.cpp
  src/core/ref_image.cpp
  src/core/model_ops.cpp)
target_include_directories(mazz_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# File formats: pure C++ too.
add_library(mazz_io STATIC
  src/io/mazz_file.cpp
  src/io/export.cpp)
target_link_libraries(mazz_io PUBLIC mazz_core)

if(MAZZ_BUILD_TESTS)
  enable_testing()
  add_executable(mazz_core_tests tests/test_core.cpp tests/test_model.cpp tests/test_io.cpp tests/test_export.cpp tests/test_model_ops.cpp)
  target_link_libraries(mazz_core_tests PRIVATE mazz_io mazz_core)
  add_test(NAME core COMMAND mazz_core_tests)
endif()

# ---------------------------------------------------------------- app
if(MAZZ_BUILD_APP)
  include(cmake/Deps.cmake)

  set(MAZZ_APP_SOURCES
    src/app/main.cpp
    src/app/editor.cpp
    src/app/file_ops.cpp
    src/app/tool_host.cpp
    src/tools/select_tool.cpp
    src/tools/prim_tool.cpp
    src/tools/handles.cpp
    src/tools/vertex_tool.cpp
    src/tools/pick_tool.cpp
    src/tools/pushpull_tool.cpp
    src/tools/selection_op_tool.cpp
    src/tools/extrude_tool.cpp
    src/tools/inset_tool.cpp
    src/app/script.cpp
    src/app/paint_window.cpp
    src/app/screenshot.cpp
    src/app/settings.cpp
    src/render/gl.cpp
    src/render/lines.cpp
    src/render/view_target.cpp
    src/render/view_draw.cpp
    src/render/model_draw.cpp
    src/render/image_draw.cpp
    src/app/ref_images.cpp
    src/ui/actions.cpp
    src/ui/theme.cpp
    src/ui/views_panel.cpp
    src/ui/op_windows.cpp
    src/ui/panels.cpp
    src/ui/imgui_checks.cpp
  )
  if(APPLE)
    list(APPEND MAZZ_APP_SOURCES src/platform/file_dialog_mac.mm src/platform/pen_mac.mm)
  else()
    list(APPEND MAZZ_APP_SOURCES src/platform/file_dialog_stub.cpp src/platform/pen_stub.cpp)
  endif()

  if(APPLE)
    add_executable(Mazzuola MACOSX_BUNDLE ${MAZZ_APP_SOURCES})
    set_target_properties(Mazzuola PROPERTIES
      MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in
      MACOSX_BUNDLE_BUNDLE_NAME "Mazzuola"
      MACOSX_BUNDLE_GUI_IDENTIFIER "app.mazzuola.modeller"
      MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
      MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
    target_compile_definitions(Mazzuola PRIVATE GL_SILENCE_DEPRECATION)
    target_link_libraries(Mazzuola PRIVATE "-framework AppKit" "-framework OpenGL")
  else()
    add_executable(Mazzuola ${MAZZ_APP_SOURCES})
  endif()

  # GLFW must not pull in the legacy OpenGL header (render/gl.hpp includes the right one).
  target_compile_definitions(Mazzuola PRIVATE MAZZ_VERSION="${PROJECT_VERSION}" GLFW_INCLUDE_NONE)
  target_include_directories(Mazzuola PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(Mazzuola PRIVATE mazz_io mazz_core mazz_imgui mazz_stb mazz_assets glfw)
endif()

# Warnings for our own code only (not third-party).
foreach(t mazz_core mazz_core_tests Mazzuola)
  if(TARGET ${t})
    target_compile_options(${t} PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wno-unused-parameter>)
  endif()
endforeach()
