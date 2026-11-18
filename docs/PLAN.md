# Mazzuola — Plan

## Current status

- **Milestone 0 (skeleton) done: v0.0.0.** Window, views, grid, navigation, menus with
  every planned shortcut. Nothing can be modelled yet.
- Waiting on the user: confirm the shortcut table (Help → Controls), and the by-hand
  check of M0 step 7 (a panel pulled out onto a second screen / iPad).
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
