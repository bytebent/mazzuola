# Mazzuola

A small, fast modeller for static low-poly 3D models: few polygons, small hand-painted
pixel textures, vertex colours. Made to work alongside Scalpellino; will export
`.glb`/`.gltf` and `.obj`.

Personal project, built milestone by milestone. See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)
for the design and [`docs/PLAN.md`](docs/PLAN.md) for progress.

**Status:** milestone 1 (mesh basics): primitives and rocks, selecting, moving / scaling /
rotating, typed transforms with copies, mirror, flatten, model pivot, reference images,
the scale figure, undo, saving `.mazz` files and exporting `.glb`/`.gltf`/`.obj`.

## Download

Builds are on the Releases page: `Mazzuola-intel.zip` for Intel Macs (Apple Silicon on
demand, see `docs/releases/INSTALL.md`).

## Controls

Everything is also in the menus, and **Help → Controls** (**F1**) lists every shortcut,
including the planned ones with the milestone that brings them.

| Action | Keys |
|---|---|
| New / open / save / save as | **Cmd+N** / **Cmd+O** / **Cmd+S** / **Cmd+Shift+S** |
| Export | File → Export → `.glb`, `.gltf` (+ `.bin`) or `.obj` (the `.mtl` comes with textures, milestone 5) |
| Undo / redo | **Cmd+Z** / **Cmd+Shift+Z** |
| Selection mode: vertices / edges / faces / pieces | **1** / **2** / **3** / **4** |
| Select | click (**Cmd+click** adds or removes), or drag a box from empty space; **Esc** selects nothing |
| Select all | **Cmd+A** |
| Grow / shrink, similar, edge loop / ring | Select menu |
| Hide selected / hide others / show all | **H** / **Shift+H** / **Cmd+U** |
| Tools: select / vertex / primitives | **Shift+S** / **Shift+V** / **Shift+B** |
| Move the selection (2D views) | drag it; moves in grid steps while snap is on |
| Scale / rotate (2D views) | drag the handles around the selection; click the selection to switch between scale and rotate handles (rotation in 15° steps while snap is on); **Esc** during a drag puts it back |
| Create a primitive or a rock | Create menu, or the Primitives tool: drag a box in a 2D view, type values in Properties; **Return** creates, **Esc** cancels |
| Move / rotate / scale / copy, typed | **Cmd+T** (around the selection, the model pivot, the origin or a typed point; copies step one after another) |
| Move points exactly | **Cmd+Shift+T** |
| Snap points to grid | **Cmd+B** |
| Duplicate / delete | **Cmd+D** / **Delete** |
| Mirror / flatten / model pivot | Mesh menu |
| Frame the selection | **Cmd+E** |
| Reference images (2D views) | View → Reference images…: per direction, size, place, turn, opacity, in front or behind |
| Scale figure (175 cm) / lines over faces in 3D | View menu |
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
| Paint window (a second window, e.g. for an iPad through Sidecar) | View → Paint window; drag on its view to turn it, two-finger scroll zooms (painting arrives in milestone 6) |
| Check pen and mouse input | Help → Pen and mouse test |
| Quit | **Cmd+Q** (asks about unsaved changes) |

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
