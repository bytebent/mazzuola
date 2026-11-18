# Mazzuola — Plan

## Current status

- **Milestone 0 (skeleton) done: v0.0.0.** Window, views, grid, navigation, menus with
  every planned shortcut. Nothing can be modelled yet.
- Waiting on the user: confirm the shortcut table (Help → Controls), and try the paint
  window test (v0.0.2) on the iPad with the Pencil.
- **Next: milestone 1 — Mesh basics**, after the user's go-ahead.

## How to build and test (AI workspace)

- GitHub archive downloads are blocked here, so FetchContent can't download. Clone the
  deps at the pinned refs (`cmake/Deps.cmake`) into a scratch folder and configure with
  `-DFETCHCONTENT_SOURCE_DIR_GLFW=… -DFETCHCONTENT_SOURCE_DIR_IMGUI=… -DFETCHCONTENT_SOURCE_DIR_STB=…`.
- Linux packages: `libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev
  libgl1-mesa-dri libxkbcommon-dev xvfb` (`apt-get update` first).
- Core tests: `./build/mazz_core_tests`. UI scripts: `tests/ui/run.sh $PWD/build/Mazzuola <shot-dir>`
  (absolute path), and again with `MAZZ_VIEW_SCALE=2` (Retina-sized view images).
- Idle check: start the app under `xvfb-run` with `HOME` set to a temp folder, wait 4 s,
  read utime+stime from `/proc/<pid>/stat`, wait 10 s, read again: the difference must be 0.

## How to work in this repo

- Read `CLAUDE.md`, `docs/ARCHITECTURE.md`, this file, then `docs/HANDOFF.md`.
- Milestone by milestone, small steps, build and test each one. Release
  `v0.<milestone>.<patch>` at the end of each milestone.
- Update this file after every step: status, test results, next steps, parked questions.
- Screenshots into `docs/screenshots/` as `m<milestone>-<nn>-<what>.png`, mentioned in
  the log.
- If a question blocks one thing, park it below and carry on with other work.
- Reports to the user: short and plain. What changed for them, what was found and fixed,
  what's next. Ask before starting the next milestone.

## Milestone 0 — Skeleton

1. Clone the Scalpellino repo and copy over what both need, adapted to GLFW/OpenGL:
   file dialogs (`platform/file_dialog*`), settings storage, `cmake/EmbedFile.cmake`
   and Roboto embedding, `cmake/Info.plist.in`, the UI script runner, CI workflows.
2. CMake project, pinned deps (GLFW, ImGui docking, stb), Roboto embedded. (No glad: a
   short own loader list in `render/gl.hpp`. cgltf comes with export in M1.)
3. Main loop on `glfwWaitEvents`: redraw only on change. Check idle CPU ≈ 0.
4. ImGui docking with multi-viewport; default layout (four views, texture view, side
   panels, status bar, counter); menus (File, Edit, Select, Create, Mesh, Texture, Paint,
   View, Help); Help → Controls.
   Light theme with red accent (ARCHITECTURE §6).
5. Views: four views with maximise, direction switching, splitters; 3D orbit around
   origin / selection and fly mode; pan/zoom; grid in cm with the keys right of P
   (steps as in Scalpellino, plus millimetre steps), grid/snap toggles.
6. Shortcut scheme from ARCHITECTURE §6 wired to menu entries; shown to the user for
   confirmation.
7. Early check (by hand): a dummy panel pulled out into its own window works on a second
   display and on an iPad through Sidecar; pressure readable through `tablet_mac.mm`.
   Only a check; the feature itself is M8.
8. CI: macOS Intel build zip per push, Linux core tests, Apple Silicon on demand.

## Milestone 1 — Mesh basics

1. Core mesh (vertices, tri/quad faces, per-corner texture coordinate and colour, part,
   smoothing group, double-sided, diagonal) and document with command-based undo.
2. Primitives: plane, box (stacks, rounded edges), cylinder, cone, tube, torus, sphere,
   geosphere; drag in a view, typed values, live preview, Return/Esc.
3. Rock generator (Create menu): faces, roughness, stretch, flat bottom, join into
   quads, seed; shared generator machinery (seed + New seed, values remembered,
   preview). Smoothing and texture coordinates for the rock follow once M3/M5 exist.
4. Selection modes 1–4 with picking pass; box select, grow/shrink, select similar,
   edge loop / ring.
5. Transform handles + Transform dialog (pivots, angle snap); clone in a row or circle,
   with rotate and scale per copy and "scale the offset"; mirror and mirror clone;
   flatten X/Y/Z; move along normal.
6. Model pivot tool.
7. `.mazz` reader/writer (`units cm` header) + round-trip golden tests.
8. Reference images per view (load, move, rotate, scale, transparency slider,
   front/back switch).
9. Scale figure toggle (175 cm).
10. Export geometry to `.glb`/`.gltf` (cm → m; units + pivot in `extras`) and `.obj`; read-back
    tests.

## Milestone 2 — Modelling

1. Shared machinery: live preview + typed-values panel, Return/Esc; automatic splitting
   of faces over four corners; inference snapping with coloured hints and Shift lock.
2. Draw: lines, rectangles, circles on faces (split the face); polygon outline in a view
   with holes, filled automatically, optional double-sided.
3. Push/pull: drag or type; cutting through a solid makes a hole.
4. Extrude (region / each face, along normals or straight, edges, keep original,
   smoothing of side faces), inset, bevel.
5. Bridge, shell.
6. Divide: subdivide (2×2, N×M; plain or smoothed), divide edges, poke, connect.
7. Cut (knife), loop cut, slide.
8. Merge, collapse edge, weld by distance, dissolve, fill, split, separate into part,
   delete, flip, flip diagonal, triangulate / join into quads, double-sided toggle.
9. Mirror mode + apply mirror.
10. Tests: validity per tool, closure tests, draw-on-face planarity, push/pull holes.

## Milestone 3 — Lathe, loft, parts, smoothing, problems

1. Lathe: profile, axis (vertical/horizontal, movable), sides, angle and slice caps,
   smoothing group per profile segment.
2. Loft: path, section (circle or edited), rounded corners, section scale/twist along
   the path, open/closed, caps.
3. Parts panel: assign, rename, hide/show/isolate, select, isolate copy; part pivots
   (placed by click or typed, used as transform centre, exported as node origin).
4. Smoothing groups panel: new, remove, auto smooth by angle; primitives assign groups.
5. Problems list: detection, jump, per-problem Fix, Fix all (one undo step).
6. Tests for all of the above, including part pivots round trip and export origins.

## Milestone 4 — Soft selection + vertex brush

1. Soft selection in the vertex tool: typed radius and curve, straight or surface
   distance, gradient display; used by move/rotate/scale.
2. Vertex brush: drag moves points within the radius with falloff; surface circle;
   optional axis/normal lock.
3. Respects selection, hidden parts and mirror mode; one stroke = one undo step; Esc
   cancels.
4. Jitter points (Mesh menu): amount, direction, keep on grid, keep open edges, seed;
   welded corners stay welded, mirror-aware.
5. Tests: falloff, distance modes, locks, mirror, hidden parts, undo; jitter limits,
   locks, mirror, seed.

## Milestone 5 — Texture coordinates

1. New blank texture / load PNG; textures panel; apply texture (Shift+T).
2. Mapping tool: planar, box, cylindrical, spherical; orientation by view or by clicked
   face; unfold (with other start face) and flatten by angle.
3. Texture view: pixel grid, linked selection, move/rotate/scale/mirror, single points,
   pixel snapping, weld/unweld, stitch, regroup, shared-area highlight, tile display.
4. Coordinates kept by extrude, divide, bevel; tests.
5. Texel check display mode, texel target, even out.
6. Transparent pixels cut out; double-sided display.
7. Image card (Create menu): size from texel target or typed width, facing, single /
   crossed pair / crossed three, base, double-sided, trim to alpha; tests.
8. File watching with reload and conflict question.
9. Layout image export (×1–×8); import scaled image.
10. Per-texture filtering (nearest default, smooth); exports with textures (embedded /
    beside), samplers per filtering, alpha, double-sided.

## Milestone 6 — Pixel painting

Pencil, line, rectangle, fill, picker, eraser (colour or transparent); palette panel and
saved palettes; painting on the texture view and on the model; paint view tab; stroke
fill-in across faces; one stroke = one undo step; warning when moving pieces with paint
under them. Generate texture (Texture menu): patterns, palette colours, colour count,
dither, tileable, seed; new PNG or replace current texture (one undo step).

## Milestone 7 — Vertex colours

Colour brush (typed radius and strength), fill selected, picker, split colours, vertex
colour display modes, exports with colours (`COLOR_0`, `.obj` extension), unlit option,
parts as nodes or merged. Scatter on faces (Mesh menu): pick target faces, copies,
spacing, align to surface, spin, size range, sink, avoid overlaps, seed; tests.

## Milestone 8 — Detached paint view + tablets

Paint view as its own window on any display, position remembered per display (docks back
if the display is gone), vertex brush available there, pen pressure for the brush's
reach and colour strength, double-tap action, larger-buttons option, two-finger camera;
hand test with a second monitor and an iPad.

## Later

- Tree generator (Create menu): trunk and branches from tapered lofts, foliage as cards,
  crossed cards or clumps; two parts (trunk, foliage); see ARCHITECTURE §6. Needs loft
  (M3), parts (M3) and textures (M5).

## Open questions (parked)

- **Bottega**: the shared home of all assets (models, textures, palettes, ...), used by
  Mazzuola and Scalpellino. Layout, how both apps browse it, live updates: to design.

- **Shared texture sheet**: several models using one sheet already works by referencing
  the same PNG; should the texture view also show other models' pieces on it?
- **N-gons**: triangles and quads only (tools split anything larger). Enough?
- **Grid steps**: match Scalpellino's cm grid steps once they're set there.
- **Texel target default**: 0.5 px/cm proposed. Confirm.
- **Texture sizes**: 16–256 suggested, larger allowed. Enforce anything?
- **Scalpellino reading `.mazz` directly** (live edits in the level) or `.glb` only?
- **Double-tap default**: picker or eraser.
- **Vertex brush and texture coordinates**: when points move, texture coordinates stay
  as they are (the texture stretches with the shape). Proposed default.
- **Shortcuts**: the proposed table in ARCHITECTURE §6 (push/pull on Shift+F, vertex
  colour on Shift+O are the least obvious) — confirm in M0.

## Log

### 2026-11-18 — Design

- Design agreed with the user and written down: `CLAUDE.md`, `README.md`,
  `docs/ARCHITECTURE.md`, `docs/PLAN.md`, `docs/HANDOFF.md`.
- Decided: own repo; code both tools need is copied from the Scalpellino repo and
  adapted (M0 step 1).
- Next: milestone 0 after the user's go-ahead.

### 2026-11-18 — Additions

- Added from the user's Bryce-inspired list: rock generator (M1), rotate/scale per copy
  in Clone (M1), jitter points (M4), image card (M5), generate texture (M6), scatter on
  faces (M7), tree generator (later). Changes made while adding them: a Create menu
  for everything that makes new geometry; generators must never produce problems
  (tested over many seeds) instead of the problems list blocking Return.
- Reference images: transparency slider plus a front/back switch. No scanning import.
- Wording: low-poly / pixel textures / vertex colours is the style; early 3D games are
  an inspiration, not a target.

### 2026-11-18 — Units and filtering

- 1 unit = 1 cm (Scalpellino is moving to cm). Scale figure 175 cm, `units cm` in
  `.mazz`, export divides by 100, examples converted.
- Texture filtering is a per-texture setting (nearest default, smooth option), shown in
  the 3D views and written to the export samplers. The texture view always shows exact
  pixels; painting always works on whole pixels.
- Parked: grid steps (match Scalpellino), texel target default (0.5 px/cm proposed).

### 2026-11-18 — Look

- UI: light theme with a red accent (Scalpellino is dark with blue). Axis colours stay
  standard; selection in the views is orange so it isn't confused with the X axis.

### 2026-11-18 — Milestone 0 done (v0.0.0)

- Built: CMake project (GLFW 3.4, Dear ImGui v1.93.0-docking with its GLFW/OpenGL 3
  backends, stb at a pinned commit; Roboto embedded); `mazz_core` (grid in cm, view
  directions, 2D and 3D cameras, grid line maths) with 280 checks in `mazz_core_tests`;
  OpenGL 4.1 core rendering of the views into multisampled offscreen images.
- Copied from Scalpellino and adapted: settings folder, file dialogs (not used until M1),
  `EmbedFile.cmake`, `Info.plist.in`, the duplicate-widget-ID check, `tests/ui/run.sh`,
  CI workflows, `INSTALL.md`.
- UI: light theme, red accent; menus File, Edit, Select, Tools, Create, Mesh, Texture,
  Paint, View, Help, with later features listed disabled and labelled with their
  milestone; one action table drives menus, shortcuts, Help → Controls and scripts.
  Four views (Top, Front, Right, 3D) with direction menus, maximise/restore, resizable
  splitters; texture view (empty checkerboard), Tools, Properties, Parts, Palette panels;
  status bar with tool, mode, cursor position, grid and the vertex/triangle counter.
- Navigation: 2D wheel zoom toward the cursor, middle / Space+left pan; 3D right-drag
  orbit, wheel zoom, pan; fly mode (Z over the 3D view; WASD, Q/E, Shift; Z or Esc
  stops). Cmd+0 back to origin. Grid keys è/+, Shift+R, Shift+W, View → Set grid size.
- Decisions made along the way: a **Tools** menu was added (tool shortcuts need a menu
  home); right mouse orbits the 3D view (Scalpellino uses it to look around: Mazzuola is
  model-centred); view selection colour is **pink** (user's choice, replaces orange).
- Tests: core 280/280; UI script `m0_views` passes at 1× and 2×; idle CPU 0 ticks
  over 10 s.
- Screenshots: `m0-01-start.png`, `m0-02-top-zoomed-panned.png`, `m0-03-orbited.png`,
  `m0-04-directions.png`, `m0-05-maximised-3d.png`, `m0-06-flying.png`,
  `m0-07-controls.png`.
- CI: the first Mac build failed inside GLFW (its Cocoa `.m` files were compiled as
  Objective-C++ because only OBJCXX was enabled); fixed by enabling OBJC too. All jobs
  green afterwards (macOS Intel build, Linux tests); release v0.0.0 published with
  `Mazzuola-intel.zip`.
- Not checked here (no Mac in the workspace): the look on a Retina screen. The
  second-screen / iPad check is by hand.

### 2026-11-18 — v0.0.1: first Sidecar test (user, by hand)

- Found by the user: (1) with the app on the iPad, Apple Pencil taps didn't press
  anything; (2) the texture view pulled out onto the iPad moved as a whole at every pen
  touch; (3) turning Sidecar off with Mazzuola on the iPad crashed it (inside ImGui's
  GLFW backend, creating a window: `glfwGetCocoaWindow` on a null window).
- Causes and fixes:
  1. A pen tap has no pointer move before it, and GLFW only updates the position on
     moves, so the click landed where the pointer was before. `platform/pen_mac.mm`
     (Cocoa local event monitor) sends the event's real position to ImGui just before
     each click, and tags pen events as pen input. It also records pressure and tilt
     (groundwork for M8).
  2. ImGui lets a window be dragged from any empty spot; now only by the title bar
     (`ConfigWindowsMoveFromTitleBarOnly`).
  3. GLFW refuses 0-sized windows and the backend then used the null window. The
     backend is compiled from a copy with sizes clamped to at least 1 px
     (`cmake/Deps.cmake`); and when a display disconnects, panels in their own windows
     are moved back over the main window (they merge into it).
- Added Help → Pen and mouse test (click count, pointer position/source, buttons, pen
  pressure/tilt, how many clicks needed their position supplied).
- Not testable here (no Mac, no iPad): the user checks the three fixes by hand.
- User re-test of v0.0.1: with the whole app on the iPad, the Apple Pencil now works.
  Still to report: a panel pulled out onto the iPad, and turning Sidecar off.

### 2026-11-18 — v0.0.2: paint window test

- User's re-test of v0.0.1: with the whole app on the iPad the Pencil works; a panel
  pulled out onto the iPad still doesn't. User proposed splitting Mazzuola into two
  apps (a paint app on the iPad, Mazzuola on the Mac) sharing the bottega folders, with
  live updates. Talked through: two apps need syncing (files or a live link), split
  undo, two bundles. Middle option chosen for a test first: one app with a **second
  real window** for painting (shares everything, no syncing).
- **Bottega** (user's term): where all assets live (models, textures, palettes...),
  shared with Scalpellino. To be designed; parked below.
- Built: `app/paint_window.*`, a second GLFW window with its own ImGui context, opened
  from View → Paint window (test). Pen-sized buttons (tools, palette, tap counter), a 3D
  view (drag orbits, since pens have no right button; scroll zooms), pen readout. Its
  own GL objects (VAOs/framebuffers aren't shared between contexts); textures are.
- The pen fix now covers every ImGui context (main and paint window). The monitor
  callback now chains to ImGui's own (it was replacing it, so ImGui could miss display
  changes).
- Tests: core 280/280; `m0_views` (now also opens/closes the paint window and checks it
  follows the grid) passes at 1× and 2×; idle 0 ticks. Screenshot `m0-08-paint-window.png`.
- If the test works on the iPad, milestone 8 uses this second window, and the ImGui
  "pull a panel out" feature can be switched off (it was the fragile part).

### 2026-11-18 — v0.0.3: paint window display test

- User's test of v0.0.2: in the paint window on the iPad, Pencil taps register, but the
  window doesn't redraw until moved back to the Mac screen.
- The one difference from the main window (which redraws fine on the iPad): the paint
  window was set not to wait for the display (swap interval 0, to avoid waiting twice
  per loop). Now it waits like the main window (interval 1). Cost: while both windows
  draw, each waits for the display, so about half the frame rate during interaction.
  Fine for the test; to revisit if this is the fix (e.g. draw a window only when it
  changed).
- Added to the paint window: a "Frames drawn" counter and a "Sync to display" checkbox
  (on = new behaviour, off = v0.0.2's) so the user can tell a presenting problem from a
  drawing one.

### 2026-11-18 — v0.0.4: paint window display test, round 2

- User's test of v0.0.3: on the iPad the paint window takes taps and keeps drawing (frame
  counter rises, seen once back on the Mac), but the iPad never shows new frames;
  "Sync to display" makes no difference. So: a presenting problem for the second
  window's OpenGL context on the Sidecar display, not a drawing one.
- Added `platform/window_surface_*` and two switches in the paint window: "Refresh
  context" (`[NSOpenGLContext update]` before each frame) and "Redisplay window"
  (`setNeedsDisplay` on its view after each swap). Asked the user to also check whether
  the main window still updates on the iPad while the paint window is open.
- If neither helps: the only setup proven to work on the iPad is one app with one window
  there. Then the fallback is the user's two-apps idea (a paint app whose only window is
  on the iPad), with syncing through the bottega files.

### 2026-11-18 — v0.0.5: iPad test, round 3

- User's test of v0.0.4: neither switch helped. And since v0.0.2 even the **main**
  window doesn't update on the iPad (it takes taps, but the picture doesn't change),
  also without opening the paint window. v0.0.1 worked.
- The only v0.0.2 change touching the main window: the GLFW monitor callback now
  chained to ImGui's own (so ImGui refreshes its monitor list on display changes).
  Undone in v0.0.5 as a test. If v0.0.5 updates on the iPad, that's the cause; if not,
  check whether v0.0.1 still works today (a change in the iPad setup?).

### 2026-11-18 — v0.0.6: iPad test, round 4

- User's test of v0.0.5: no change; **v0.0.1 doesn't update on the iPad now either, nor
  does Scalpellino**, while Safari, Discord etc. do. So it isn't a Mazzuola change: our
  OpenGL windows aren't shown on the Sidecar display (the earlier v0.0.1 success remains
  unexplained).
- Suspected cause: dual-GPU MacBook Pro; GLFW (and raylib) by default create OpenGL
  contexts without "offline renderers" and the app doesn't declare automatic graphics
  switching, so the context stays tied to one GPU and can't present on a display driven
  differently (Sidecar's virtual display).
- v0.0.6: `GLFW_COCOA_GRAPHICS_SWITCHING` on (adds `NSOpenGLPFAAllowOfflineRenderers`)
  for the main and paint windows, and `NSSupportsAutomaticGraphicsSwitching` in
  Info.plist. The monitor callback stays unchained (as in v0.0.1) until this is settled.
- If this works, Scalpellino needs the same (raylib sets its own window hints: needs a
  look there).
