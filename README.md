# Mazzuola

A small, fast modeller for static low-poly 3D models: few polygons, small hand-painted
pixel textures, vertex colours. Made to work alongside Scalpellino; will export
`.glb`/`.gltf` and `.obj`.

Personal project, built milestone by milestone. See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)
for the design and [`docs/PLAN.md`](docs/PLAN.md) for progress.

**Status:** alpha 1 (milestone 1, redone) is complete with v0.1.4 "Out of the app":
draw every primitive shape and rocks; select whole pieces, faces, edges or points; move,
copy, resize and rotate them like brushes, along the arrows, or by typed values
(Transform panel, Multi-Replicate, mirror, flatten); undo, save and open `.mazz`; export
`.glb`, `.gltf` and `.obj`; reference images in the 2D views; a 175 cm scale figure.
Milestone 2 (modelling) is planned next.

## Download

Builds are on the Releases page: `Mazzuola-intel.zip` for Intel Macs (Apple Silicon on
demand, see `docs/releases/INSTALL.md`).

## Controls (so far)

Everything is also in the menus, and **Help → Controls** (**F1**) lists every shortcut,
including the planned ones with the milestone that brings them.

| Action | Keys |
|---|---|
| New / open / save / save as | **Cmd+N** / **Cmd+O** / **Cmd+S** / **Cmd+Shift+S** |
| Export (.glb, .gltf + .bin, .obj + .mtl; Y-up, metres) | File → Export |
| Export again (same file and format) | **Cmd+Shift+E** |
| Undo / redo | **Cmd+Z** / **Cmd+Shift+Z** |
| Tools (also the buttons on the left) | Select **Shift+S**, Vertex **Shift+V**, Primitives **Shift+B** |
| Selection mode: points / edges / faces / whole pieces | **1** / **2** / **3** / **4** (and the buttons at the top of the tool bar); switching converts the selection |
| Draw a shape | Primitives tool (or Create → Primitives / Rock): pick the shape and its settings in Properties, drag a rectangle in a 2D view, adjust with the handles in any 2D view (or type the size); **Return** creates, **Esc** cancels. Round shapes stand along the view's depth (drawn in Top = upright); the depth comes from the last shape made or selected |
| Select | Select tool: click a piece (again on the same spot: the one underneath); **Cmd+click** adds/removes; drag from empty space for a rectangle; **Esc** selects nothing; **Cmd+A** all |
| Move / copy | drag inside the selection's box in a 2D view (snapped); **Shift+drag** copies |
| Resize | drag a side or corner handle of the selection's box (the opposite side stays) |
| Rotate | **Option+drag** a corner handle (15° steps, add **Shift** for free); the rings in the 3D view |
| Move along one axis | drag an X/Y/Z arrow in the 3D view (points, edges, faces) |
| Drag a point | Vertex tool: press on any point and drag (in a 2D view all the points stacked there move) |
| Edge loop / ring | double-click an edge / **Shift**+double-click (also in the Select menu) |
| Grow / shrink selection | **Cmd++** / **Cmd+è** |
| Delete | **Delete** (in point or edge mode: the faces that use them) |
| Typed move / rotate / scale | **Cmd+T** (a window that stays open; the views show it live; **Return** applies, **Esc** cancels) |
| Multi-Replicate (copies in a row, circle, spiral...) | **Cmd+Shift+D** (same kind of window) |
| Duplicate in place | **Cmd+D** |
| Mirror / mirror clone / flatten / snap points to grid | Mesh menu; snap **Cmd+B** |
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
| Reference image (2D views) | **Image...** in the view's header; while its window is open, drag the picture to move it, a corner to resize it; width, rotation, transparency, behind / in front in the window. Shared by views facing the same way; not saved |
| Scale figure (175 cm) | View → Scale figure |
| Paint window (a second window, e.g. for an iPad through Sidecar) | View → Paint window; drag on its view to turn it, two-finger scroll zooms (painting arrives in milestone 6) |
| Check pen and mouse input | Help → Pen and mouse test |
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
