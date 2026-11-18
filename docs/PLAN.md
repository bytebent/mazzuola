# Mazzuola — Plan

## Current status

- Design written (`docs/ARCHITECTURE.md`). Nothing built yet.
- **Next: milestone 0 — Skeleton**, after the user's go-ahead.

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
2. CMake project, pinned deps (GLFW, ImGui docking, glad, cgltf, stb), Roboto embedded.
3. Main loop on `glfwWaitEvents`: redraw only on change. Check idle CPU ≈ 0.
4. ImGui docking with multi-viewport; default layout (four views, texture view, side
   panels, status bar, counter); menus (File, Edit, Select, Mesh, Texture, Paint, View,
   Help); Help → Controls.
5. Views: four views with maximise, direction switching, splitters; 3D orbit around
   origin / selection and fly mode; pan/zoom; grid with the keys right of P, grid/snap
   toggles.
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
3. Selection modes 1–4 with picking pass; box select, grow/shrink, select similar,
   edge loop / ring.
4. Transform handles + Transform dialog (pivots, angle snap); clone in a row or circle;
   mirror and mirror clone; flatten X/Y/Z; move along normal.
5. Model pivot tool.
6. `.mazz` reader/writer + round-trip golden tests.
7. Reference images per view (load, move, rotate, scale, opacity).
8. Scale figure toggle.
9. Export geometry to `.glb`/`.gltf` (units + pivot in `extras`) and `.obj`; read-back
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
4. Tests: falloff, distance modes, locks, mirror, hidden parts, undo.

## Milestone 5 — Texture coordinates

1. New blank texture / load PNG; textures panel; apply texture (Shift+T).
2. Mapping tool: planar, box, cylindrical, spherical; orientation by view or by clicked
   face; unfold (with other start face) and flatten by angle.
3. Texture view: pixel grid, linked selection, move/rotate/scale/mirror, single points,
   pixel snapping, weld/unweld, stitch, regroup, shared-area highlight, tile display.
4. Coordinates kept by extrude, divide, bevel; tests.
5. Texel check display mode, texel target, even out.
6. Transparent pixels cut out; double-sided display.
7. File watching with reload and conflict question.
8. Layout image export (×1–×8); import scaled image.
9. Exports with textures (embedded / beside), nearest samplers, alpha, double-sided.

## Milestone 6 — Pixel painting

Pencil, line, rectangle, fill, picker, eraser (colour or transparent); palette panel and
saved palettes; painting on the texture view and on the model; paint view tab; stroke
fill-in across faces; one stroke = one undo step; warning when moving pieces with paint
under them.

## Milestone 7 — Vertex colours

Colour brush (typed radius and strength), fill selected, picker, split colours, vertex
colour display modes, exports with colours (`COLOR_0`, `.obj` extension), unlit option,
parts as nodes or merged.

## Milestone 8 — Detached paint view + tablets

Paint view as its own window on any display, position remembered per display (docks back
if the display is gone), vertex brush available there, pen pressure for the brush's
reach and colour strength, double-tap action, larger-buttons option, two-finger camera;
hand test with a second monitor and an iPad.

## Open questions (parked)

- **Shared texture sheet**: several models using one sheet already works by referencing
  the same PNG; should the texture view also show other models' pieces on it?
- **N-gons**: triangles and quads only (tools split anything larger). Enough?
- **Texture sizes**: 16–256 suggested, larger allowed. Enforce anything?
- **Scalpellino reading `.mazz` directly** (live edits in the level) or `.glb` only?
- **Double-tap default**: picker or eraser.
- **Vertex brush and texture coordinates**: when points move, texture coordinates stay
  as they are (the texture stretches with the shape, PS1-like). Proposed default.
- **Shortcuts**: the proposed table in ARCHITECTURE §6 (push/pull on Shift+F, vertex
  colour on Shift+O are the least obvious) — confirm in M0.

## Log

### 2026-11-18 — Design

- Design agreed with the user and written down: `CLAUDE.md`, `README.md`,
  `docs/ARCHITECTURE.md`, `docs/PLAN.md`, `docs/HANDOFF.md`.
- Decided: own repo; code both tools need is copied from the Scalpellino repo and
  adapted (M0 step 1).
- Next: milestone 0 after the user's go-ahead.
