# Mazzuola — Working Plan

**Purpose:** let any future session (human or AI) pick up work without outside context.
Update this file at the end of every step: status, what's next, open questions. The
step-by-step log is in [`LOG.md`](LOG.md) (newest entries at the top).

Design: [`ARCHITECTURE.md`](ARCHITECTURE.md). Rules: [`../CLAUDE.md`](../CLAUDE.md).

---

## Current status

- **Released:** v0.0.0 … v0.0.7 (milestone 0, the skeleton, incl. the iPad check);
  alpha 1 = milestone 1: v0.1.0 "Boxes", v0.1.1 "All shapes", v0.1.2 "Points, edges,
  faces", v0.1.3 "Typed values", v0.1.4 "Out of the app"; alpha 2 = milestone 2:
  v0.2.0 "Push and pull", v0.2.1 "Edges and points" (confirmed good), v0.2.2 "Draw on
  faces" (agreed 2026-11-20; "just go ahead, worst case we revert"; its own tool),
  v0.2.3 "Outlines and holes".
- **Released v0.2.3 "Outlines and holes"** (user: "seems like it is done") and
  **v0.2.4 "Cut and divide"**; **v0.2.5 "Clean up"** released 2026-11-21. Next:
  v0.2.6 "Bevel, bridge, mirror", to be proposed click by click.
- **Audit fixes** (2026-11-20, merged into main, no version release): release workflow,
  committed build folders, glTF export pair, view fallback without anti-aliasing,
  dependency hashes. See LOG.md. The release fix gets its first real run at v0.2.3.
- Milestone 1 was first built in one go and withdrawn (v0.1.0, reverted 2026-11-19; see
  LOG.md), then redesigned with the user click by click (ARCHITECTURE §4–§6, §9, §10).
- **Decided 2026-11-20 (user, after v0.1.4):** reference images stay on New / Open
  (they belong to the views); Export again is remembered only while the model is open.
- **Waiting on the user:** confirm the shortcut table (Help → Controls).
- **Roadmap:** M1 mesh basics → M2 modelling → M3 lathe, loft, groups, smoothing, problems
  → M4 soft selection, vertex brush → M5 texture coordinates → M6 pixel painting → M7
  vertex colours → M8 paint window + tablets; later: tree generator. Details below and in
  ARCHITECTURE §14.
- **Bottega** (shared folder with Scalpellino): being built in Scalpellino first.
  Mazzuola connects to it once it's done there (user, 2026-11-19); meanwhile other work
  goes ahead.
- How the user likes to work: one release per step, short plain reports, ask before
  starting the next milestone (see `HANDOFF.md`).

## How to work in this repo

Build, tests, the UI script language, CLI flags, the idle check, CI, releasing and the
per-step checklist are in [`DEV.md`](DEV.md). One step per release: add
`docs/releases/vX.Y.Z.md` and push to `main`.

## Pinned dependencies

| Dep | Version |
|---|---|
| GLFW | 3.4 (commit 7b6aead9fb88b3623e3b3725ebb42670cbe4c579) |
| cgltf | 1.15 (vendored in `third_party/cgltf`, commit 360db1a95480fe102ae9c69b27c5d101167ff5ba) |
| Dear ImGui | v1.93.0-docking (commit b916186e66aa3c02b72c6ca17455516512ed8049, with its GLFW and OpenGL 3 backends) |
| stb | commit 9600522de53973336fa033a289858a4354cbb04c |

Downloads are checked against the SHA-256 in `cmake/Deps.cmake`.


## Open questions (parked)

- **Old build folders in history** (user's call): `build-core/` and `build-dbg/` were
  committed from alpha 1 until the `audit-fixes` branch removed them. They hold AI
  workspace build paths and a Linux version string (no details of the user's Mac).
  Removing them from history means rewriting it (e.g. `git filter-repo --path build-core
  --path build-dbg --invert-paths`) and force-pushing main and the tags; release tags
  would point at new commits.





- **Shared texture sheet**: several models using one sheet already works by referencing
  the same PNG; should the texture view also show other models' pieces on it?
- **N-gons**: triangles and quads only (tools split anything larger). Enough?
- **Texel target default**: 0.5 px/cm proposed. Confirm.
- **Texture sizes**: 16–256 suggested, larger allowed. Enforce anything?
- **Double-tap default**: picker or eraser.
- **Vertex brush and texture coordinates**: when points move, texture coordinates stay
  as they are (the texture stretches with the shape). Proposed default.
- **Shortcuts**: the proposed table in ARCHITECTURE §6 (push/pull on Shift+F, vertex
  colour on Shift+O are the least obvious) — confirm in M0.

## Bottega (shared folder) — decided with the user in Scalpellino's docs, not built

The shared home of all assets, used by Mazzuola, Scalpellino and later tools. Decided so
far (from Scalpellino's PLAN.md, 2026-11-18):
- Layout: `textures/` (the library), `models/` (**one folder per model**), `maps/`,
  `palettes/`, `skies/`, `shaders/`, `renders/`, `presets/`.
- A normal folder anywhere; each tool asks once, in its settings, where it is; an empty
  folder is offered the sub-folders. A small marker file at the root with a format
  version.
- Paths are stored relative to the Bottega root (`textures/Bricks_24`, `models/tree`), so
  moving Bottega breaks nothing.
- A model folder keeps its own textures (self-contained) and may also use the shared
  `textures/` library.
- **No automatic or constant rescanning** (not even on window focus): a **Rescan**
  command (menu entry, button, shortcut). Idle stays free.
- The user's own extra folders keep working beside Bottega.
- `palettes/`: format decided when Mazzuola gets there (milestone 6).
- Scalpellino's milestone 10 (Props) loads `.glb` first and `.mazz` once Mazzuola's
  format is final.

**For Mazzuola (user, 2026-11-19): wait.** Bottega is being built in Scalpellino first;
once it's done, see how it works there and connect Mazzuola to it then. Until then
Mazzuola saves and opens with normal file dialogs. Ideas to check against Scalpellino's
version when the time comes (not agreed): a model saved as `models/<name>/<name>.mazz`
with its PNGs beside it; shared-library textures written with a Bottega path; saved
palettes in `palettes/`.
- **Exception, decided by the user:** a texture painted in another app **does** reload
  when you switch back to Mazzuola (ARCHITECTURE §6). That's one file being edited, not
  a library scan, so Bottega's "no rescanning on focus" rule doesn't apply to it.

## Milestone 0 — Skeleton (done, v0.0.0 … v0.0.7)

Delivered: CMake project and pinned deps; `mazz_core` (grid in cm, view directions, 2D
and 3D cameras, grid lines) with tests; light theme with red accent; menus with every
planned shortcut (later ones disabled, labelled with their milestone) and Help →
Controls; four views with direction menus, maximise and splitters; texture view, Tools,
Properties, Groups and Palette panels (empty until their milestones); status bar;
navigation (2D zoom/pan, 3D orbit/zoom/pan, fly mode, back to origin); grid keys and
typed grid size; scripted UI tests, idle check, CI and releases.
After the user's iPad tests (v0.0.1 … v0.0.7): pen taps click where the pen touches;
the **paint window** (a second real window, View → Paint window) works on the iPad with
the Pencil; graphics switching is on (required for Sidecar); panels can't be pulled out
of the main window. See LOG.md and ARCHITECTURE §8.

## Milestone 1 — Mesh basics: **alpha 1** (the redo)

"Alpha 1" names the redo, to tell it apart from the withdrawn first milestone 1 (LOG.md
2026-11-19). The other milestones keep their names. Design: ARCHITECTURE §6 ("Brush
feel", Selection, Vertex tool, Primitives, Transform, reference images). Five releases,
each tried by the user before the next starts:

1. **v0.1.0 — Boxes.** Foundations (tool classes like Scalpellino's, clean numbers,
   undo, selection with its mode); left tool bar and status hints; Primitives tool with
   box only; selecting whole shapes; selection box: move, resize, rotate (corners in 2D,
   rings in 3D), Shift+drag copy; delete; undo; save and open `.mazz`.
   **Steps of v0.1.0** (each one built, tested and committed before the next; see
   "How to check each step" below):
   - **A. Core foundations.** Clean-number rule (1/1000 cm; exact sines/cosines at
     multiples of 90°); mesh (points, tri/quad faces with the fields of §5); selection
     with its mode; pieces (connected faces); document with commands that store only
     what they change, plus the selection before/after; saved/unsaved marker that undo
     respects. Core tests for all of it.
   - **B. `.mazz` files.** Writer and reader in the §5 number format (plain decimals,
     never `1e+02`); clear errors with line numbers for bad files; File → New / Open /
     Save / Save as, with "save changes?" when needed. Round-trip test, and open a
     written file and read it by eye.
   - **C. Tool structure and layout.** `Tool` interface and routing (§4); remove the
     Tools panel; left tool bar with Select and Primitives only; Vert / Edge / Face /
     Piece buttons at its top (Piece on; the other three greyed out until v0.1.2);
     status bar shows the active tool's hint. Rename the Parts panel to Groups.
   - **D. Drawing the model.** Drawing data built once per change and shared by the
     views (shaded faces in 3D, edges in 2D); selection and handles as a separate
     overlay. Idle check stays at zero.
   - **E. Box primitive.** Primitives tool with box only: drag a rectangle in a 2D view
     (snapped); depth from the last shape made or selected (none yet: 64 cm, starting
     at 0 on that axis, so a box drawn in Top stands on the ground); preview in all
     views with the brush handles in every 2D view; width/depth/height typed in
     Properties; Return creates it (selected), Esc cancels. Box is closed, faces point
     outward, corners on the grid.
   - **F. Select tool.** A click picks a whole piece (2D: on its outline or inside;
     clicking again on the same spot steps to the next one underneath; 3D: the one in
     front); Cmd+click adds/removes; drag on empty space = rectangle; empty click or
     Esc = nothing.
   - **G. Selection box.** Shared handles (§4): always visible in 2D views; drag inside
     moves (snapped); side handles push one side, opposite side fixed (snapped);
     Option+drag a corner rotates about the selection centre (15° steps, +Shift free);
     rings in the 3D view; Shift+drag copies; Delete. Each action is one undo step.
   - **H. Release.** Help → Controls and README in sync, release notes
     `docs/releases/v0.1.0.md`, screenshots, LOG. The withdrawn v0.1.0 release and tag
     must be gone from GitHub first (the user deletes them; otherwise CI won't make the
     release). Then the user tries it; their feedback comes before v0.1.1.

   **How to check each step:** DEV.md's per-step checklist (build, core tests, UI
   scripts at 1× and 2×, idle check), plus: look at real output by eye (a saved
   `.mazz`, a screenshot of the views), and never loosen a test to make it pass — fix
   the code. Questions that come up while building go to "Open questions" below and
   the work continues.

2. **v0.1.1 — All shapes.** Cylinder, cone, tube, torus, sphere, geosphere, plane; rock
   (same flow, its settings, New seed).
   **Steps of v0.1.1:**
   - **A. Shape generators (core).** One function per shape filling a box exactly,
     standing along a chosen axis (the view's depth axis when drawn), outward faces,
     closed (plane: one flat grid), clean numbers, sensible smoothing groups (round
     sides one group, flat caps flat). Box gets segments per side. Settings per §6:
     plane rows/columns/diagonal; box segments X/Y/Z; cylinder sides, segments along,
     cap rings; cone the same; tube outer sides, inner size, segments; torus sides,
     tube sides (its thickness is the box size along its axis); sphere sides and
     stacks; geosphere iterations. Tests: every shape × every axis fits its box, is
     closed (not the plane), faces outward, numbers clean; counts per setting.
   - **B. Primitives tool with all shapes.** Shape list and the chosen shape's settings
     in Properties, remembered between uses; the preview is the real shape (drawn in
     all views); the Create menu lists the shapes (opens the tool with that shape).
   - **C. Rock (Create menu).** Same flow (drag a box, adjust, Return); settings: faces
     (12–60), roughness, stretch X/Y/Z, flat bottom (with height), join into quads,
     seed + New seed (a fresh seed each time, not the same series every launch).
     Hull of jittered points on an ellipsoid, then pushed in/out along normals without
     folding over; fills its box. Tests: closed, outward, fits its box, same seed →
     same rock, different seeds differ.
   - **D. Release v0.1.1.** UI script drawing each shape in a 2D view and checking its
     box and counts; screenshots; controls, README, release notes. User tries it.

3. **v0.1.2 — Points, edges, faces.** Design: ARCHITECTURE §6 Selection ("Points,
   edges, faces"), Vertex tool, "Brush feel" (arrows).
   **Steps of v0.1.2:**
   - **A. Selection core.** Mode conversion (pieces/faces/edges/points, agreed rules);
     picking one point / edge / face (2D: stacked points together, nearest face first,
     click again for the next; 3D: what is hit); rectangle per mode (2D sees through,
     3D only visible); grow/shrink; edge loop (through four-sided corners) and ring;
     Delete for every mode (removes the faces using the selection). Core tests.
   - **B. Select tool in every mode.** Modes 1–4 enabled (tool bar buttons, keys);
     click / Cmd+click / rectangle / double-click loop / Shift+double-click ring; the
     selection box works for any selection (move, copy, resize, rotate, rings). Copy
     (Shift+drag) of points/edges copies the faces they fully cover. Cmd+è / Cmd++.
   - **C. Arrows.** X/Y/Z arrows at the selection's centre (two per 2D view, three in
     3D); drag one to move along that axis only, snapped.
   - **D. Vertex tool (Shift+V).** Press on any point and drag it straight away
     (stacked points in 2D together; snapped); otherwise like Select in point mode.
   - **E. Rounded edges.** Box and cylinder: rounded edges with 1–3 faces per edge
     (and the size of the rounding); still fills its box; tests.
   - **F. Release v0.1.2.** UI script through all of it; screenshots; controls, README,
     release notes. User tries it.

4. **v0.1.3 — Typed values.** Design: ARCHITECTURE §6 Transform, Multi-Replicate,
   Duplicate, Mirror, Flatten (agreed 2026-11-20). Also: the Vertex tool works on points
   only (2/3/4 in it switch to the Select tool).
   **Steps of v0.1.3:**
   - **A. Core operations.** Transform about a pivot (move, rotate X/Y/Z, scale X/Y/Z %,
     move along the selection's normal); replicate (N copies, per-copy offset, turn,
     scale, scaled offset, pivot travelling / world origin / typed point); duplicate;
     mirror in place and mirror clone across the world plane (faces turned the right
     way round); flatten to average / lowest / highest. All on points of any selection
     mode, clean numbers. Core tests.
   - **B. Operation windows.** The shared "operation with preview" (ARCHITECTURE §4): a
     small window that stays open, its values shown live in the views through a live
     edit, Return applies as one undo step, Esc cancels; only one open at a time. The
     Transform panel (Cmd+T) on it.
   - **C. Multi-Replicate** (Cmd+Shift+D) on the same window.
   - **D. Menu actions.** Cmd+D Duplicate; Mesh → Mirror X/Y/Z, Mirror clone X/Y/Z;
     Flatten (window: axis, to average / lowest / highest); the Vertex tool points-only
     rule.
   - **E. Release v0.1.3.** UI script through all of it; screenshots; controls, README,
     release notes. User tries it.

5. **v0.1.4 — Out of the app.** `.glb`/`.gltf` + `.obj` export (geometry; origin =
   world origin); reference images; 175 cm figure. Design: ARCHITECTURE §5 (reference
   images, figure) and §7 "Alpha 1 export" (agreed 2026-11-20, "yes to all").
   **Steps of v0.1.4:**
   - **A. Export (io).** cgltf vendored. `.glb`, `.gltf` + `.bin`, `.obj` + `.mtl`:
     Z-up cm → Y-up m, flat normals, quads split by their diagonal, one grey material,
     units in `extras`. Written through a temporary file. Tests read the files back
     (cgltf; the `.obj` parsed): counts, positions in metres, normals, winding.
   - **B. Export in the app.** File → Export (three formats, file dialog);
     Cmd+Shift+E exports again to the same file and format (asks for a file the first
     time); script command; UI script.
   - **C. Reference images.** From a 2D view's header: load (PNG, JPG, TGA, BMP),
     remove, a pop-up with width in cm, rotation, transparency, in front / behind;
     position and size with the brush handles (proportions kept) while the pop-up is
     open; not pickable while modelling; one image per direction, shared by views
     facing the same way; none in 3D; not saved. Script commands; UI script.
   - **D. Scale figure.** View → Scale figure: a blocky see-through 175 cm person on
     the ground beside the origin (−X side, outside the model's box), in all views,
     never exported or saved.
   - **E. Release v0.1.4.** UI scripts, idle scene with reference image and figure,
     screenshots; controls, README, release notes. Look at an exported file by eye.

Before each of v0.1.1–v0.1.4 starts, write its steps here the same way as v0.1.0's,
taking in the user's feedback on the release before.

## Milestone 2 — Modelling: **alpha 2**

**Releases (agreed with the user 2026-11-20).** Each is tried before the next is
planned click by click:
- **v0.2.0 "Push and pull"**: push/pull, extrude, inset (design: ARCHITECTURE §6,
  agreed 2026-11-20), tool buttons in the Tools panel.
- **v0.2.1 "Edges and points"** (agreed 2026-11-20, after v0.2.0): Push/pull in edge
  and point mode as an along-normal dragger (ARCHITECTURE §6 point 6); Inset doesn't
  start again on its result after Return.
- **v0.2.2 "Draw on faces"** (agreed 2026-11-20, ARCHITECTURE §6 Draw): rectangles,
  circles and lines on faces, snapping with coloured hints, direction lock.
- **v0.2.3 "Outlines and holes"** (agreed 2026-11-20, ARCHITECTURE §6 Draw): push/pull
  through a drawn shape makes a hole (window first, then doorways touching an edge);
  Outline in Draw (2D views) with holes; double-sided.
- **v0.2.4 "Cut and divide"** (agreed 2026-11-21, ARCHITECTURE §6 Cut / Slide /
  Divide): Cut tool (knife, loop cut), slide (Alt+drag in the Vertex tool), Mesh menu:
  subdivide, divide edges, poke, connect.
- **v0.2.5 "Clean up"** (agreed 2026-11-21, ARCHITECTURE §6): merge (centre / first
  selected), collapse, weld by distance, dissolve, Delete dissolving in point / edge /
  face mode (the reminder below; Tear open keeps the old Delete), fill, detach, flip,
  flip diagonal,
  triangulate / join into quads, double-sided.
- **v0.2.6 "Bevel, bridge, mirror"**: bevel, bridge, shell, mirror mode.
- Moved to milestone 3: separate into group (needs groups), smoothing of extruded side
  faces (needs smoothing groups).

**Steps of v0.2.0:**
- **A. Core operations** (`core/model_ops`): push/pull of one face (stretch the
  neighbours at right angles, else new side faces; forced new faces), extrude faces
  (region / each, along normals / straight, keep original), extrude open edges, inset
  (region / each, width, depth, "too wide" check). Tests: valid mesh, closed stays
  closed, winding outward, exact positions, counts.
- **B. Push/pull tool** (Shift+F, button): hover, drag along the normal (3D any face,
  2D edge-on faces), grid steps, typed distance, Return / Esc, release keeps the
  preview, a new push applies the pending one, Cmd = new faces. UI script.
- **C. Extrude tool** (Shift+E, button): Properties, live preview, arrow handle,
  Return / Esc, result selected. UI script.
- **D. Inset tool** (Shift+I, button): Properties, live preview, red when too wide,
  inner faces selected. UI script.
- **E. Release v0.2.0**: Tools menu entries, controls, README, release notes,
  screenshots, idle check.

**Reminder (user, 2026-11-19):** consider making Delete in point/edge mode dissolve
automatically (keep the surface closed) instead of removing the faces around.

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
8. Merge, collapse edge, weld by distance, dissolve, fill, split, separate into group,
   delete, flip, flip diagonal, triangulate / join into quads, double-sided toggle.
9. Mirror mode + apply mirror.
10. Tests: validity per tool, closure tests, draw-on-face planarity, push/pull holes.

## Milestone 3 — Lathe, loft, groups, smoothing, problems

1. Lathe: profile, axis (vertical/horizontal, movable), sides, angle and slice caps,
   smoothing group per profile segment.
2. Loft: path, section (circle or edited), rounded corners, section scale/twist along
   the path, open/closed, caps.
3. Groups panel: assign, rename, hide/show/isolate, select, isolate copy; group pivots
   (placed by click or typed, used as transform centre, exported as node origin).
4. Smoothing groups panel: new, remove, auto smooth by angle; primitives assign groups.
5. Problems list: detection, jump, per-problem Fix, Fix all (one undo step).
6. Tests for all of the above, including group pivots round trip and export origins.

## Milestone 4 — Soft selection + vertex brush

1. Soft selection in the vertex tool: typed radius and curve, straight or surface
   distance, gradient display; used by move/rotate/scale.
2. Vertex brush: drag moves points within the radius with falloff; surface circle;
   optional axis/normal lock.
3. Respects selection, hidden groups and mirror mode; one stroke = one undo step; Esc
   cancels.
4. Jitter points (Mesh menu): amount, direction, keep on grid, keep open edges, seed;
   welded corners stay welded, mirror-aware.
5. Tests: falloff, distance modes, locks, mirror, hidden groups, undo; jitter limits,
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
groups as nodes or merged. Scatter on faces (Mesh menu): pick target faces, copies,
spacing, align to surface, spin, size range, sink, avoid overlaps, seed; tests.

## Milestone 8 — Paint window + tablets

The paint window (a second real window, already working on the iPad since v0.0.6) gets
the full painting UI: pixel and vertex colour painting, the vertex brush, palette; pen
pressure for the brush's reach and colour strength; double-tap action; position
remembered; draw only the window that changed (both windows wait for the display);
hand test with a second monitor and an iPad.

## Later

- Tree generator (Create menu): trunk and branches from tapered lofts, foliage as cards,
  crossed cards or clumps; two groups (trunk, foliage); see ARCHITECTURE §6. Needs loft
  (M3), groups (M3) and textures (M5).

## Log

The step-by-step log is in [`LOG.md`](LOG.md), newest first.
