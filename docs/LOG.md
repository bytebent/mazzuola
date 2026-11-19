# Mazzuola — Log

What was done, step by step, newest first. Recent work lives here; when this file gets
long, move the oldest entries to `LOG-archive.md` (newest first there too). Status, plan
and open questions are in [`PLAN.md`](PLAN.md).

### 2026-11-20 — v0.1.3 design agreed; steps planned (docs only)
- Agreed (user: "yes to all"): Transform panel (Cmd+T) and Multi-Replicate
  (Cmd+Shift+D) as two windows like Scalpellino's, staying open with a live preview,
  Return / Esc; relative values about the selection centre, world origin or a typed
  point; Mirror X/Y/Z in place, Mirror clone X/Y/Z across the world plane; Flatten to
  average / lowest / highest; move along normal in the Transform panel; Cmd+D Duplicate.
  ARCHITECTURE §6 and the shortcut table updated; PLAN: steps of v0.1.3 (A–E).

### 2026-11-20 — Vertex tool: points only (docs only)
- User: in the Vertex tool with a face selected, pressing a corner handle grabbed the
  corner point (points come first in that tool) instead of resizing. Decided: the
  Vertex tool works on points only; 2/3/4 in it switch to the Select tool. With v0.1.3.

### 2026-11-20 — After v0.1.2: arrows in 3D only; views ordered like Scalpellino
- User (screenshot): the arrows in the 2D views got in the way. They now show in the 3D
  view only; in 2D the selection box and its handles move things as before.
- User: default views 3D, Top / Front, Right, as in Scalpellino. View numbers in every
  UI script moved with them (0 = 3D, 1 Top, 2 Front, 3 Right).
- The arrow test moved to the 3D view, with a script helper `varrow` (mouse on an
  arrow). Its first version aimed at the arrow of the already-moving selection, so the
  mouse ran ahead; it now keeps the arrow where it was when the button went down.
- Properties counted faces in every mode ("1 faces"); now points / edges / faces.
- ARCHITECTURE (arrows, layout diagram), README, Help → Controls updated. All UI
  scripts at 1× and 2× and idle pass. In "latest"; goes out with v0.1.3.

### 2026-11-20 — v0.1.2, steps E–F: rounded edges; release
- Rounded edges: box (flat faces + a strip of 1–3 faces along each edge + a corner patch
  of q² triangles, built at the real proportions so the rounding is the same size in cm
  on every edge) and cylinder (rounded rims, 1–3 faces, same rule). Settings "Rounded
  edges" (0–3) and "Rounding" (% of half the smallest side, up to 90); box segments
  greyed out while rounded. Script `primset rounded | rounding`.
- Version 0.1.2, release notes, README status and controls, Help → Controls rows.
- Tests: core 650 checks (every shape loop with rounding 0/1/2; rounded box and
  cylinder counts, closed, outward, filling their box for q = 1–3 and every axis;
  6.4 cm rounding on a 64 x 32 x 48 box measured). `a1_shapes` checks a rounded box's
  face count; all seven UI scripts at 1× and 2×; idle 0 ticks. Screenshot
  `m1-08-rounded.png`.

### 2026-11-19 — v0.1.2, steps A–D: points, edges, faces; arrows; Vertex tool
- `core/select_ops`: mode conversion (agreed rules); picking points (2D: every point
  stacked on the nearest spot; 3D: nearest visible), edges and faces (nearest to the
  viewer first); rectangles per mode (2D sees through; 3D only visible points, faces
  turned toward the camera); grow/shrink; edge loop (straight through four-sided
  corners) and ring (opposite sides across quads); faces Delete removes per mode.
- Select tool in every mode: click / Cmd+click / click again for the next one /
  rectangle / double-click an edge = loop, Shift+double-click = ring; Shift+drag copies
  the faces the selection covers and selects the copy in the same mode. Modes 1–4 and
  the tool bar buttons enabled; switching converts. Select menu: grow (Cmd++), shrink
  (Cmd+è), edge loop, edge ring. Delete in every mode. Points drawn in point mode.
- Arrows: X/Y/Z at the selection's centre (2D: the view's two; 3D: three unless one
  points at the viewer); drag = move along that axis only, snapped (corner on grid).
- Vertex tool (Shift+V, tool bar): a press on any point grabs it straight away (stacked
  points in 2D together; an already-selected point moves the whole selection).
- Fixes found while testing: a single point's selection box has no size, so pressing
  it hit the corner handles and dragging did nothing: tiny boxes now have no handles
  and a press on them moves. The status bar ran out of room with the longer hints:
  the right part (selection, cursor, grid, counts) now always shows, the hint ends in
  "…" if needed.
- Tests: core 613 checks (conversion, picking, rectangles 2D/3D, grow/shrink, loops and
  rings on a cylinder, delete). New UI script `a1_parts`; `m0_views` and `a1_layout`
  back to testing keys 1–4; all pass. Screenshot `m1-07-faces.png`.

### 2026-11-19 — Loops and grow/shrink keys; v0.1.2 planned (docs only)
- Edge loop = double-click an edge, ring = Shift+double-click (explained to the user with
  a drawing). Shrink / grow selection = Cmd+è / Cmd++ (user: easier on the Italian
  keyboard; matches è/+ for the grid). Shortcut table and Selection updated.
- PLAN: steps of v0.1.2 (A selection core, B Select tool in every mode, C arrows,
  D Vertex tool, E rounded edges, F release).

### 2026-11-19 — v0.1.2 design answers (docs only)
- Rock stretch dropped (the drawn box sets the proportions).
- Agreed for points/edges/faces: mode switches convert the selection; a click picks one
  thing (stacked points together in 2D, faces nearest first, click again for the next);
  rectangle sees through in 2D, only visible in 3D; Vertex tool = grab any point
  straight away, Select = click first; Delete removes the faces using the selection.
  Reminder for milestone 2: maybe dissolve automatically on Delete. ARCHITECTURE §6.

### 2026-11-19 — Rounded edges decided (docs only)
- User: rounded edges on box and cylinder are fine if small: a hard edge becomes 1–3
  faces, not 8. Planned with v0.1.2; ARCHITECTURE updated.

### 2026-11-19 — v0.1.1 "All shapes" released
- Version 0.1.1, release notes, README status and controls, Help → Controls ("Draw a
  shape"). Parked: rounded edges (box, cylinder) and rock stretch, both in ARCHITECTURE
  but not built.
- Checks: core 587; six UI scripts at 1× and 2×; idle 0 ticks.

### 2026-11-19 — v0.1.1, step C: the rock
- `core/rock`: points spread on a sphere (Fibonacci) and jittered by roughness, their
  convex hull (incremental, own code); a flat bottom by cutting the hull with a level
  plane (edge crossings added, points below dropped; clamping points onto the plane
  sometimes left no real bottom); dents pushed inward along normals, halved until no
  face turns over; optional join into quads (neighbours within 20°, convex); round part
  in smoothing group 1, bottom flat. The number of starting points is chosen so the
  triangle count comes closest to the one asked (with a flat bottom, 24 asked gave ~36
  before; now 22–26). Fills its box like every shape. Same seed, same rock.
- The rock is one more kind in the Primitives tool (same draw → adjust → Return), with
  its own Create menu entry. Settings: faces, roughness, flat bottom + height, join into
  quads, seed + New seed (a fresh random seed, not the same series each launch).
  **Stretch X/Y/Z left out:** the drawn box sets the proportions, so stretching would
  be undone by fitting the box (question for the user in PLAN).
- Tests: core 587 checks (rocks in the every-shape loop; determinism; 200 random rocks
  closed, outward, fitting, no flat faces; triangle count). `a1_shapes` draws a rock;
  all scripts at 1× and 2×; idle 0 ticks. Screenshot `m1-06-rock-settings.png`.

### 2026-11-19 — v0.1.1, steps A–B: all primitive shapes
- `core/primitives`: box (segments per side), plane (rows, columns, flipped
  diagonals), cylinder and cone (sides, segments, cap rings), tube (sides, segments,
  hole %), torus (sides, tube sides, tube width %), sphere (sides, stacks), geosphere
  (iterations). Each is built in its own unit space, then mapped exactly onto the drawn
  box, so every shape fills its box for any number of sides (the old torus didn't).
  Polygons have flats facing the axes when the sides are a multiple of 4. Round sides
  share a smoothing group, caps are flat. A plane lies on the box's floor, facing +axis.
- Primitives tool: shape list and the shape's settings in Properties, remembered per
  shape; the preview is the real shape (edges in all views) with the box handles;
  Create → Primitives opens the tool with a shape. Depth from the last shape, but a flat
  last shape (a plane) gives the default 64 cm (found by the new script: after a plane,
  every shape came out flat).
- Script commands `prim`, `primset`, `expect previewfaces`.
- Tests: core 564 checks: every shape × every axis × three settings variants fits its box
  exactly, is closed (not the plane), faces outward, has clean numbers, no zero-area
  faces, one piece; counts. New UI script `a1_shapes` draws every shape and checks box
  and face count, a 12-sided cylinder, a torus drawn in Front; all scripts pass.
  Screenshot `m1-05-shapes.png`.

### 2026-11-19 — Move snaps the corner onto the grid; v0.1.1 planned
- User tried v0.1.0: "looks good". Decided: a drag in the 3D view stays a selection
  rectangle; moving keeps things on the grid: the selection box's lowest corner snaps
  onto it (as Scalpellino does), so off-grid things snap on. `moveOffset` changed (Select
  and Primitives tools); `a1_boxes` checks an off-grid box (x −77) landing on −64.
  ARCHITECTURE "Brush feel" updated; the two open questions closed.
- PLAN: steps of v0.1.1 (A shape generators, B Primitives tool with all shapes, C rock,
  D release).

### 2026-11-19 — v0.1.0 "Boxes" published
- The user deleted the withdrawn v0.1.0 tag (and with it the release); this push lets CI
  publish the new v0.1.0 from `docs/releases/v0.1.0.md`. (Deleting releases isn't
  allowed from the AI workspace: HTTP 403. Ask the user, as here.)

### 2026-11-19 — Alpha 1, step H: v0.1.0 "Boxes" prepared
- Version 0.1.0; release notes `docs/releases/v0.1.0.md`; README status and controls,
  Help → Controls (mouse gestures of the tools added) in sync.
- The withdrawn v0.1.0 release and tag are still on GitHub, so CI won't publish this one
  yet: once they're deleted, the next push to `main` creates it. Meanwhile the "latest"
  pre-release has this build.
- Checks: core 412; all five UI scripts at 1× and 2×; idle 0 ticks.

### 2026-11-19 — Alpha 1, steps E–G: box primitive, select tool, selection box
- `core/transform`: stretch points between box sides, turn about a centre (exact at
  90°), angle snapping, copy faces. `core/pick`: pieces under a point (2D: inside a face
  or near an edge, nearest to the viewer first; 3D: the face hit first) and pieces
  entirely inside a rectangle. Tested.
- `tools/handles`: the shared box handles (corners and sides in 2D views; Option turns
  corners into rotation), X/Y/Z rings in 3D, resizing (opposite side fixed, snapped,
  never thinner than a grid step), snapped move offsets, drag angles. Ring angles are
  measured where the mouse ray meets the ring's plane (measuring on screen gave 105°
  for a 90° drag in perspective).
- Primitives tool, box only: drag a rectangle in a 2D view (snapped), depth from the
  last shape made or selected (default z 0..64, so a box drawn in Top stands on the
  ground); dashed preview in all views with handles in the 2D views; size typed in
  Properties; Return creates it selected, Esc drops it.
- Select tool: click picks a whole piece (clicking again on the same spot steps to the
  next one underneath), Cmd+click adds/removes, a rectangle from empty space picks
  pieces entirely inside, an empty click selects nothing. Selection box: drag inside
  moves (snapped offset), Shift+drag copies (the copy is selected), side/corner
  handles resize, Option+drag a corner turns about the centre (15° steps, Shift free),
  rings in 3D; Esc during a drag puts everything back. Each is one undo step (a live
  edit). Delete, Select all (Cmd+A) and Select none (Esc) enabled.
- Script commands `vworld`, `hold`/`letgo`, `expect bounds | selbounds | preview`.
- Tests: core 412 checks (also under ASan/UBSan). New UI script `a1_boxes` runs the
  whole v0.1.0 flow (draw, adjust height in Front, Return, Esc, click, Cmd+click,
  rectangle, move, copy, resize, Option-rotate, ring rotate, delete, undo, save) and
  checks the saved file by line; all scripts pass at 1× and 2×; idle 0 ticks at 1× and
  2× (control 867). Screenshots `m1-03-box-made.png`, `m1-04-handles.png`.

### 2026-11-19 — Alpha 1, step D: drawing the model
- `core/draw_data`: triangles with flat normals and each edge once, built once per
  geometry change; index lists of the selected triangles/edges. The `Document` now has
  change counters (`meshVersion`, `selVersion`); every `MeshEdit` change, undo, redo and
  reset moves them.
- `render/model_draw`: the model on the GPU, shared by all views. 3D: shaded faces
  (light over the viewer's shoulder), dark edges, selected faces tinted and their edges
  pink. 2D: edges, selected edges pink. Each frame the editor compares the counters with
  what it drew: geometry changed → rebuild and upload; only the selection changed → just
  the two index lists; neither → nothing (views aren't redrawn).
- GL loader: `glDrawElements`, `glPolygonOffset`, `glUniform1f/3f/4f` added (Linux).
- Fixed the macOS CI build of step B's test (`mkdtemp` → `std::filesystem`).
- Tests: core 398 checks (draw data counts, outward normals, selection index lists,
  counters). New UI script `a1_draw`; all scripts pass at 1× and 2×. The idle scene now
  holds two boxes (one selected): 0 ticks at 1× and 2× (control 855). Screenshot
  `docs/screenshots/m1-02-two-boxes.png`.

### 2026-11-19 — Alpha 1, step C: tool structure and layout
- `src/tools/tool.hpp`: the `Tool` interface (press, drag, release, hover, key, overlay,
  properties, cancel, busy), `Pointer`, `ToolEnv`, `LastBox`; `SelectTool` and
  `PrimTool` (hints only so far; behaviour in steps E–G). The editor routes left mouse
  (not Space+left) and keys to the active tool first, draws its overlay over each view
  image (clipped), shows its Properties section, and cancels it before a tool switch,
  undo, redo, New and Open (`app/tool_host.cpp`).
- `core/view_geom`: per-view projection and picking rays (3D agrees with the camera
  matrices; 2D rays run along the view direction), ray–triangle test. Tested.
- Layout: the Tools panel is gone; a tool bar on the left (Vert / Edge / Face / Piece at
  the top, Vert–Face greyed out until v0.1.2; then Select and Primitives). Status bar:
  tool name and its gestures on the left, selection / cursor / grid / counts on the
  right. Parts panel renamed Groups. README controls updated.
- Tests: core 384 checks. UI scripts `a1_files`, `a1_layout` (new: tool switching by
  key and action, greyed modes), `m0_views` pass at 1× and 2×; idle 0 ticks at 1× and
  2×. Screenshot `docs/screenshots/m1-01-layout.png`.

### 2026-11-19 — Alpha 1, step B: `.mazz` files
- `io/mazz_file` (new `mazz_io` library, pure C++): writer and reader of format 1 (§7
  minus textures/groups/palettes, which come later); plain decimals; attributes and
  `uv`/`col` lines only when not default; strict reader with "line N: why" errors;
  saving writes `<file>.saving` then renames it.
- Editor holds a `Document`. File → New / Open / Save / Save as work (native dialogs on
  macOS, a typed-path dialog elsewhere), "Save changes?" (Save / Don't save / Cancel)
  before New, Open and Quit (also the window's close button); errors in a message box;
  window title shows the file name and • when unsaved. Undo/Redo enabled. Status bar
  counts points and triangles. Modes 1–3 greyed out until v0.1.2.
- Script commands `saveas`, `open`, `addbox` (test helper) and `expect dirty | undo |
  faces | verts | selected | title | fileline` (DEV.md). New UI script `a1_files`.
- Tests: core 375 checks (golden file text for a box with a noisy height and a stray
  point; 300-point random round trip, also through a real file; 14 bad files each
  rejected at the right line). UI scripts `a1_files`, `m0_views` pass at 1× and 2×
  (`m0_views` updated: keys 1–3 now leave the mode on pieces). Idle: 0 ticks at 1× and
  2× (control 874). Checked by eye: a saved box reads `v 100 50 62.5`, no `1e+02`.

### 2026-11-19 — Alpha 1, step A: core foundations
- `core/numbers`: clean numbers (1/1000 cm, `-0` → 0), exact cos/sin at multiples of
  90°, plain-decimal writing (`100`, `62.5`, never `1e+02`) that reads back exactly.
- `core/mesh`: points, tri/quad faces (fields of §5), face normal/centre, pieces
  (union-find), `addBox` (closed, outward, cleaned).
- `core/selection`: selection with its mode, whole pieces, selected points, bounds.
- `core/document`: `MeshEdit` records only the points/faces it touches; a step stores
  that change plus the selection before/after; one-shot `apply` and live edits
  (`beginLive`/`commitLive`/`cancelLive`) for drags; saved marker that undo respects;
  history limited to 64 MB.
- Tests (`tests/test_model.cpp`, helpers in `tests/check.hpp`): 349 checks pass, also
  under AddressSanitizer/UBSan with -Wall -Wextra (no warnings). Includes: 20 000
  random values round-trip through text exactly; undo after a mode switch restores the
  selection and its mode; moving one point of a 500-box model stores < 1 KB.

### 2026-11-19 — v0.1.0 step plan (docs only)
- PLAN.md: the steps of v0.1.0 (A core foundations, B `.mazz` files, C tool structure
  and layout, D drawing, E box primitive, F select tool, G selection box, H release),
  how each step is checked, and the rule to write each later release's steps before it
  starts. HANDOFF updated.

### 2026-11-19 — Foundations for alpha 1 written down (docs only)
- Reference images are not remembered at all (user): gone when the app closes.
- ARCHITECTURE: tool classes behind one `Tool` interface (as in Scalpellino), one
  shared "operation with preview", shared handles/rings/arrows (§4); clean numbers
  (1/1000 cm, plain decimals in `.mazz`) and selection holding its mode (§5); drawing
  data built once per change and shared by the views, overlays separate (§9); undo
  storing only what changed, history limited by memory (§10). Fixed §4's diagram (no
  multi-viewport).

### 2026-11-19 — Redesign: pivot, reference images, alpha 1 releases (docs only)
- Vertex drag in 2D takes all stacked points, as in Scalpellino.
- No model pivot tool: the world origin is the model's pivot (and the export's origin).
- Reference images: one per orthographic view, chosen from that view (not the View
  menu), options in a pop-up panel; not saved in the `.mazz`.
- Alpha 1 = the milestone 1 redo (milestones keep their names), in five releases
  v0.1.0 – v0.1.4 (PLAN.md).

### 2026-11-19 — Redesign: transform panel, rings, arrows, vertex drag (docs only)
- Agreed: the Transform dialog becomes a panel that stays open with a live preview
  (Return applies, Esc cancels), like Scalpellino's Multi-Replicate. Rotation is about
  the selection's centre by default; rotation rings show in the 3D view too. Points,
  edges and faces get X/Y/Z arrows to push along an axis. Vertex tool: click and drag
  one point straight away, as in Scalpellino.
- Agreed: milestone 1 becomes **alpha 1**, split into a reasonable number of releases
  (list being agreed).

### 2026-11-19 — Parts are now "groups" (docs only)
- User: keep Scalpellino's language. Parts (named face sets with a pivot, visibility,
  exported as nodes) renamed **groups**, made with Cmd+G, removed with Cmd+Shift+G, as in
  Scalpellino. No separate visgroups. "Piece" (everything connected) keeps its name.
  Renamed throughout ARCHITECTURE, PLAN and README; the code (empty Parts panel, `part=`
  in the planned file format) follows when it's next touched.

### 2026-11-19 — Redesign: making a shape (docs only)
- Agreed: primitives click by click like Scalpellino's Block tool (shape and settings in
  Properties; drag a rectangle, depth from the last shape; preview with brush handles in
  every 2D view and typed sizes; round shapes fill their box; Return creates, Esc
  cancels; clean numbers). After Return a primitive is plain mesh (user).

### 2026-11-19 — Redesign: selecting, tool bar (docs only)
- Agreed: selecting click by click (a click picks a whole shape by default; 1–3 finer,
  4 back; click again to reach what's underneath in 2D; Cmd+click; rectangle on empty
  space; empty click / Esc selects nothing; selection box in every view).
- Agreed: the main tools as buttons on the left, as in Scalpellino; no panel listing
  every tool. ARCHITECTURE §6 layout and Selection updated.
- Then agreed: the bar shows only tools that work in the current release; Vert / Edge /
  Face / Piece buttons at its top (Piece by default); the status bar lists the active
  tool's gestures, as in Scalpellino.

### 2026-11-19 — Redesign: brush feel (docs only)
- Agreed with the user: models are points and faces, not brushes, but shapes are
  handled like Scalpellino's brushes, also once the model gets complex: draw a box →
  adjustable preview → Return; selection box with always-visible handles; side handles
  push one side (opposite side fixed, snapped); Option+drag a corner to rotate (15°,
  +Shift free); drag moves, Shift+drag copies. Written as ARCHITECTURE §6 "Brush feel".

### 2026-11-19 — Milestone 1 withdrawn (v0.1.0), back to the design
- Milestone 1 (`43e0a5d`, `0b93748`, released as v0.1.0) was built by Claude Sonnet by
  mistake instead of Opus. The user tried it: apart from the basic UI elements
  (dropdowns, selecting, typing), everything felt wrong. Both commits reverted; the code
  is back to `5aa4703` (end of milestone 0). The old code stays in history for parts
  worth reusing (glTF/OBJ export, `.mazz` reader, picking maths).
- Found in review (to avoid next time): no tool structure (all tools in one `Editor`
  class, unlike Scalpellino's `Tool` classes); undo kept two full copies of the model per
  step; the selection mode lived outside undo (undo after a mode switch lost the
  selection); hiding faces was saved and undoable; every change rebuilt all four views;
  dialogs with no live preview; the `.mazz` writer wrote 100 as `1e+02` and kept
  rounding noise (`3e-15` instead of 0); the torus didn't fill its box; round shapes
  faced the view they were drawn in, with the depth taken from a size you can't see;
  tests were loosened to fit the code instead of the other way round.
- Next: design first, with the user (how each basic action works, click by click),
  then foundations, then small releases the user tries. The v0.1.0 release on GitHub
  has to be removed by hand (the workspace can't).

### 2026-11-19 — Two decisions (docs only)
- Textures painted in another app reload when switching back to Mazzuola, as designed
  (user). Open question closed.
- Bottega: Mazzuola waits until it's built in Scalpellino, then connects to it (user).
  Meanwhile normal file dialogs; the earlier Mazzuola-side Bottega proposals are kept
  in PLAN.md only as ideas to check then.

### 2026-11-18 — Docs restructured like Scalpellino's; idle check harness
- Docs split the way Scalpellino's now are: `CLAUDE.md` (rules), `ARCHITECTURE.md`
  (design), `PLAN.md` (status, milestone plans, parked questions, Bottega), `LOG.md`
  (this file; the log moved out of PLAN.md), `DEV.md` (build, tests, script language,
  idle check, CI, releasing, checklist, workspace quirks), `HANDOFF.md` (reading order,
  the user, decisions to keep, mistakes).
- Bottega as decided in Scalpellino's docs, applied to Mazzuola (PLAN.md "Bottega"),
  with the Mazzuola-specific points proposed or parked.
- Idle check like Scalpellino's: `tests/ui/idle.sh` + `tests/ui/idle/scene.script`
  (views changed, Controls window and paint window open, no `quit`); `--control` mode.
  Scripts now stay open when they end without `quit` (busy only while commands are left);
  errors print as `FAIL script: …`. CI's linux job runs control, 1× and 2×.
- Grid steps settled: Scalpellino (now in cm) kept powers of two, 1–512 cm plus 0.125–0.5
  as microgrid; Mazzuola already uses 0.125–512 cm, so they match (open question closed).
- Scalpellino's milestone 10 will load `.mazz` directly once its format is final (open
  question "Scalpellino reading `.mazz`" closed).
- Results: core 280/280; `m0_views` passes; idle 0 ticks at 1× and 2×; control 898 ticks
  in 5 s.

### 2026-11-18 — v0.0.6 works; v0.0.7 cleanup

- User's test of v0.0.6: **it works**: the whole app and the paint window update on the
  iPad, and take the Pencil. Cause confirmed: graphics switching (see ARCHITECTURE §8).
  Scalpellino has the same problem on Sidecar but the user won't use it there: no change.
- v0.0.7 cleanup: ImGui multi-viewport off (panels stay in the main window), so the
  backend size patch, the monitor callback and "bring windows home" are gone; the
  paint window's test switches (frame counter, sync, refresh, redisplay) and tap test are
  gone (`platform/window_surface_*` removed). The paint window stays (View → Paint
  window) as a shell for milestones 6 and 8: tool and palette buttons (not wired), 3D
  view, pen readout. Help → Pen and mouse test stays.
- Docs: ARCHITECTURE §8 rewritten (paint window, why no pulled-out panels, graphics
  switching, pen taps); CLAUDE.md and HANDOFF note the graphics-switching rule.
- Tests: core 280/280; `m0_views` at 1× and 2×; idle 0 ticks.
- Milestone 0 is complete (the step 7 check is done). Next: milestone 1 after the user's
  go-ahead.

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

### 2026-11-18 — v0.0.5: iPad test, round 3

- User's test of v0.0.4: neither switch helped. And since v0.0.2 even the **main**
  window doesn't update on the iPad (it takes taps, but the picture doesn't change),
  also without opening the paint window. v0.0.1 worked.
- The only v0.0.2 change touching the main window: the GLFW monitor callback now
  chained to ImGui's own (so ImGui refreshes its monitor list on display changes).
  Undone in v0.0.5 as a test. If v0.0.5 updates on the iPad, that's the cause; if not,
  check whether v0.0.1 still works today (a change in the iPad setup?).

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

### 2026-11-18 — Look

- UI: light theme with a red accent (Scalpellino is dark with blue). Axis colours stay
  standard; selection in the views is orange so it isn't confused with the X axis.

### 2026-11-18 — Units and filtering

- 1 unit = 1 cm (Scalpellino is moving to cm). Scale figure 175 cm, `units cm` in
  `.mazz`, export divides by 100, examples converted.
- Texture filtering is a per-texture setting (nearest default, smooth option), shown in
  the 3D views and written to the export samplers. The texture view always shows exact
  pixels; painting always works on whole pixels.
- Parked: grid steps (match Scalpellino), texel target default (0.5 px/cm proposed).

### 2026-11-18 — Additions

- Added from the user's Bryce-inspired list: rock generator (M1), rotate/scale per copy
  in Clone (M1), jitter points (M4), image card (M5), generate texture (M6), scatter on
  faces (M7), tree generator (later). Changes made while adding them: a Create menu
  for everything that makes new geometry; generators must never produce problems
  (tested over many seeds) instead of the problems list blocking Return.
- Reference images: transparency slider plus a front/back switch. No scanning import.
- Wording: low-poly / pixel textures / vertex colours is the style; early 3D games are
  an inspiration, not a target.

### 2026-11-18 — Design

- Design agreed with the user and written down: `CLAUDE.md`, `README.md`,
  `docs/ARCHITECTURE.md`, `docs/PLAN.md`, `docs/HANDOFF.md`.
- Decided: own repo; code both tools need is copied from the Scalpellino repo and
  adapted (M0 step 1).
- Next: milestone 0 after the user's go-ahead.
