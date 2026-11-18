# Mazzuola

A small, fast modeller for static low-poly 3D models: few polygons, small hand-painted
pixel textures, vertex colours. Made to work alongside Scalpellino; will export
`.glb`/`.gltf` and `.obj`.

Personal project, built milestone by milestone. See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)
for the design and [`docs/PLAN.md`](docs/PLAN.md) for progress.

**Status:** milestone 0 (the skeleton): the window, views, grid and navigation. Nothing
can be modelled yet; that starts in milestone 1.

## Download

Builds are on the Releases page: `Mazzuola-intel.zip` for Intel Macs (Apple Silicon on
demand, see `docs/releases/INSTALL.md`).

## Controls (so far)

Everything is also in the menus, and **Help → Controls** (**F1**) lists every shortcut,
including the planned ones with the milestone that brings them.

| Action | Keys |
|---|---|
| Selection mode: vertices / edges / faces / pieces | **1** / **2** / **3** / **4** |
| Smaller / larger grid | **è** / **+** (the two keys right of P) |
| Set the grid size | View → Set grid size… (1/8 cm to 512 cm; **Return** sets, **Esc** cancels) |
| Show grid / snap to grid | **Shift+R** / **Shift+W** |
| Zoom a view | mouse wheel (2D views zoom toward the cursor) |
| Pan a view | hold **middle mouse**, or **Space + left mouse** |
| Orbit the 3D view | hold **right mouse** and drag (around the origin; header buttons choose origin or selection) |
| Fly mode (3D view) | **Z** while hovering the 3D view, or the **Fly** button; **W A S D** move, **Q/E** down/up, hold **Shift** for fast; **Z** or **Esc** stops |
| Change a view's direction | the menu in the view's header: Top, Bottom, Front, Back, Left, Right, 3D |
| Maximise / restore a view | **Maximise** / **Restore** in the view's header, or View → Maximise view under the cursor |
| Resize the views | drag the bars between them; double-click a bar to centre it |
| Back to the origin | **Cmd+0** (all views) |
| Reset the window layout | View → Reset layout |
| Quit | **Cmd+Q** |

Keys are read by physical position, so they work the same on any keyboard layout.
Units are centimetres; Z is up.

## Building locally

Needs CMake 3.24+ and a C++17 compiler. Dependencies (GLFW, Dear ImGui, stb) are fetched
at configure time at pinned versions.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
ctest --test-dir build --output-on-failure   # core tests
```

On macOS this makes `build/Mazzuola.app`. On Linux (used for automated tests) install
`libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libxkbcommon-dev
xvfb`; the scripted UI tests run with `tests/ui/run.sh "$PWD/build/Mazzuola" [shot-dir]`.
