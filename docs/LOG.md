# Mazzuola — Log

What was done, step by step, newest first. Recent work lives here; when this file gets
long, move the oldest entries to `LOG-archive.md` (newest first there too). Status, plan
and open questions are in [`PLAN.md`](PLAN.md).

### 2026-11-24 — v0.2.7 steps B–C: mirror mode in the app, Make symmetric window
- `Document`: `mirrorAxis` / `mirrorSide`; `select()` takes in the twins; a live edit
  keeps the twins from its start and `mirrorFixLive()` mirrors its changes; `apply()`
  mirrors one-shot changes and twins their selection. `MeshEdit::renumbered()`: an
  edit that dropped points (Bevel, Dissolve…) is not fixed up (it worked on the twin
  selection, so both sides changed alike).
- Editor (`app/mirror_mode.cpp`): Mesh → **Mirror mode** (Shift+M, checked), **Mirror
  plane X / Y / Z** (picking one turns it on), **Make symmetric…**. The side being
  worked on: the selected point (or any point) nearest the mouse at a press. The fix-up
  runs after each tool press, drag and key, and after each operation window preview.
  The plane: a dashed line in the axis colour in 2D views that see it edge-on, a
  see-through rectangle round the model in 3D. Status bar: "mirror X" and how many
  points have no twin. Push/pull pushes the twin face (or points) the same distance
  along its own normal; a hole's twin is found again by its faces' centres.
- **Make symmetric…** window: Plane X / Y / Z, Keep +side / -side (starting from the
  mirror mode's plane and the side last worked on); live preview; red with the reason.
- Tests: core 1810 (Document: twin selection, a one-shot move, a live drag on -X with
  undo, a bevel through the twin selection). `a2_mirror` (new): twin pick, Push/pull
  both sides out (exact bounds), a Vertex-tool drag mirrored, Move / rotate / scale
  mirrored, a box only on +X (8 points without a twin) made symmetric, an off-centre box
  kept +X / -X, off again. 20 scripts pass.
- Screenshot `m2-16-mirror.png` (top corners dragged out on +X, the -X side follows;
  the plane dashed in Top and Front, see-through in 3D).

### 2026-11-24 — v0.2.7 step A: mirror mode core
- `core/mirror_ops`: **twinPoints** (pairs by position, rounded to the stored 1/1000 cm;
  points on the plane are their own twins; points sharing a spot pair in order),
  **unpairedPoints**, **twinFaces**, **withTwins** (points, edges, faces), **mirrorFix**
  (after a change made on one side: each changed point there puts its twin at its
  mirror; points that were on the plane keep that coordinate at 0; new points left
  alone), **makeSymmetric** (faces across the plane cut along it, faces lying in it
  dropped, the kept half mirror-cloned and welded along the plane; refuses an empty
  side, a face crossing twice, or opening a closed model). `MeshEdit::vertChanged` /
  `oldVertCount` added (where a point was before the edit).
- Tests: core 1802 checks (twins of a box across X / Y, touching and apart; twin
  selection of a point, edge, face, a face across the plane; the fix for a move, both
  sides moved alike, the -X side, a 30° turn, a point on the plane, a point without a
  twin; Make symmetric both ways with exact bounds and volume, a face in the plane, a box
  apart, the empty side refused, a 7-sided cylinder made symmetric across Y). Clean
  under the sanitizers.

### 2026-11-22 — v0.2.6 "Bevel, bridge, shell" released
- Help → Controls: a Bevel row (Bridge… and Shell… come from the menu table). README:
  Bevel and Bridge / Shell rows; the status paragraph was behind (no v0.2.5), now up
  to v0.2.6. Release notes `docs/releases/v0.2.6.md`. Version 0.2.6 (export check).
- Checks: core 1682; 19 UI scripts pass at normal, 2× and no anti-aliasing; idle 0
  ticks at 1× and 2× (control 903). A saved `.mazz` of one edge bevelled with 2
  segments looked at by eye: clean numbers (the arc point 2.343 / 45.657), the end caps
  took the arc as two quads, all quads.

### 2026-11-22 — v0.2.6 step C: Bridge… and Shell… windows
- Mesh menu **Bridge…** (Segments 1–32, Twist; faces: two faces or blocks; edge mode:
  an edge on each of two holes) and **Shell…** (Thickness, Inward / Outward / Centred;
  works on the pieces of the selection), in place of the "Bevel, bridge, shell"
  placeholder. Small windows like Weld's: live preview, red "Can't: …" with the reason,
  Return / Esc; the new faces (Shell: the whole shelled piece) end up selected.
  Script: `opset segments|twist|thickness|side`; `expect opvalid` also covers an open
  operation window.
- `a2_bridge_shell` (new): two boxes' facing sides (a rectangle round the gap) bridged,
  3 segments, twist, Return: 22 faces, closed; the sides torn open and the holes
  bridged (edge mode; nothing is selected after Tear open); a top-less box shelled 4
  inward and outward (exact boxes), closed; a closed box refused, Return does nothing.
  19 scripts pass; core 1682.
- Screenshots: `m2-14-bridge.png` (the tube, 3 segments), `m2-15-shell.png` (the walls).

### 2026-11-22 — v0.2.6 step B: Bevel in the Extrude tool
- Properties: **Extrude / Bevel** at the top (each greyed out when the selection can't
  use it). Edges with faces on both sides, or points, switch to Bevel when picked up
  (new `SelectionOpTool::picked` hook); faces and open edges stay Extrude. Width (cm),
  Segments (1–3; points: one, with a note). The arrow points into the surface and sets
  the width (grid steps). Red with the reason when it can't; the new faces end up
  selected. Script: `toolset extrude bevel|width|segments`, `expect bevelling`.
- `a2_bevel` (new): one edge picked in 3D (Bevel by itself, 9 faces), arrow dragged to
  width 8 (exact selection box), 2 segments, too wide red, Return; every edge (a
  rectangle; Cmd+A picks the piece) rounded with 2 segments, closed; every corner cut;
  faces: Bevel can't be chosen. `a2_extrude`: points now bevel (was "points don't"),
  as agreed. All 18 scripts pass.
- Screenshots: `m2-12-bevel-edge.png` (one edge chamfered; the end caps, five corners,
  are split in two: a diagonal shows in the 2D views, as with Draw and Cut),
  `m2-13-bevel-all.png` (every edge, 2 segments).
- Noted while testing: after Undo the tool's preview comes back at once (as Extrude
  always did), and Cmd+A selects the piece, so "every edge" is a rectangle in edge mode.

### 2026-11-22 — v0.2.6 step A: bevel, bridge, shell (core)
- `core/bevel_ops`: **bevelEdges** (edges with faces on both sides; at each point the
  faces round it are walked: between two bevelled edges a face gets the crossing of the
  offset lines, across plain edges new points slide along them at width / sin(angle);
  strips with a round profile for 2–3 segments, tangent to both faces; corner patches
  where ends meet; a flat end keeps the old point on the strip's end; points made at
  one spot are shared), **bevelPoints** (corner cut off, width along each edge),
  **bridgeFaces / bridgeHoles** (best match of the two outlines plus twist, rings for
  segments, uneven counts spread with triangles), **shell** (point normals weighted by
  angle, scaled for even thickness; inward / outward / centred; rim; closed pieces
  refused). "Too wide": faces that would turn over, or points sliding past an edge.
  `MeshEdit` unchanged; `regionOutline` now public in clean_ops.
- Probes found and fixed while writing: corner patches were judged by the faces round
  them (a bevel ending where the surface goes on straight ends in a triangle standing
  across it); arc points at one spot weren't shared (compared unrounded); flat ends.
- Tests: core 1682 checks (one edge with exact volumes for 1–3 segments, a corner,
  all 12 edges, the top loop, flat ends, too wide, points, bridges with segments, a
  tunnel, uneven counts, holes, shell three ways and an open box; 200 random bevels:
  191 closed, 9 refused with a reason). Clean under address / undefined-behaviour
  sanitizers.

### 2026-11-22 — v0.2.6 and v0.2.7 agreed
- Proposal (bevel in Extrude, Bridge…, Shell…, mirror mode with twin selection and Make
  symmetric) answered "yes to all": split into v0.2.6 "Bevel, bridge, shell" and v0.2.7
  "Mirror mode". Recorded in ARCHITECTURE §6 and PLAN (steps of v0.2.6).

### 2026-11-21 — v0.2.5.1 fix release (code review of v0.2.5)
- Review of v0.2.5 with probes (small programs against `libmazz_core.a`). Found and fixed:
  - **Face Dissolve / Delete didn't join:** the points left along the old shared edges
    stayed, so two halves of a split top stayed two faces (a 2x2 block four). Now those
    points go when they lie in a straight line (0.002 cm) between their outline
    neighbours, and the faces beyond them (a side split by Subdivide) are joined back
    without them (`dropStraightPoint`: the faces on each side of the line join; if a
    side can't, the point stays a corner). The old test only checked "closed".
  - **Face Dissolve merged the wrong points:** a lone face's points were kept as
    indices from before other points were removed; 2 of 6 box cases shrank the wrong
    face. `MeshEdit::deleteFaces` now also reports how points were renumbered
    (`vertMap`), and the points are followed through it.
  - **Point Dissolve found points by position:** after Detach two points share a spot
    and the other one went. Followed by index now.
  - **Subdivide read past a list** (v0.2.4; found by an address-sanitizer run) when
    neighbours would split a shared edge differently. It was refused anyway; it now
    stops as soon as the clash is seen.
- Smaller findings parked in PLAN.md (texture coordinates dropped by rebuilt faces,
  Fill without group, Delete's undo name, open pieces).
- Tests: core 1164 checks (new: halves and 2x2 joins back to the plain box, a block plus
  a lone face on each of the 6 sides, Dissolve after Detach; the random test also
  dissolves a face with its neighbour, 143 rounds closed), also clean under address and
  undefined-behaviour sanitizers; `a2_clean` now splits the top 2x2 and Deletes it (fails
  on v0.2.5: 13 faces). 17 scripts pass at normal, 2x and no anti-aliasing; idle 0 ticks
  (control 898). Screenshot `docs/screenshots/m2-11-joined-top.png` (the joined top,
  selected; 8 points).
- Version 0.2.5.1 (the bundle's short version stays three numbers); DEV notes fix-release
  numbering and the sanitizer build.

### 2026-11-21 — v0.2.5 "Clean up"
- `core/clean_ops`: `mergePoints` (at a point, or onto a kept point), `collapseEdges`
  (joined edges to their middle), `weldByDistance` (clusters within the distance),
  `dissolvePoints` (a point between two straight edges leaves its faces; otherwise the
  faces round it join without it), `dissolveEdges` (the faces on both sides join; the
  ends left between two straight edges go too, so a loop cut comes out whole),
  `dissolveFaces` (selected neighbours join; a face alone shrinks to its centre),
  `fillFace` (3 or 4 points, turned to match the edges round it), `detachFaces`,
  `flipFaces`, `flipDiagonals`, `triangulate` (the split as drawn), `joinIntoQuads`
  (flat, convex pairs, best first), `setTwoSided`. A tidy step after merging drops faces
  left without area and pairs of faces back to back. Merging, collapsing and dissolving
  refuse when a closed model would end up open (found by the random test: collapsing an
  edge near a sphere's pole pinched it).
- The document remembers the first selected point (Merge at first selected point).
- UI: Mesh menu items for all of them (Weld by distance... is an operation window with
  the distance and how many points go); Delete dissolves in point / edge / face mode and
  deletes in piece mode; Tear open keeps the old Delete; a message says why when one
  can't be done. Help → Controls, README, DEV updated.
- Tests: core 1125 checks (merge, collapse, weld, dissolve after a loop cut / subdivide,
  fill after tear open, detach, flips, triangulate and join back, first selected point,
  random clean-ups on a box, cylinder and sphere: 121 rounds closed); UI script
  `a2_clean` (and `a1_parts` now checks Delete dissolves, Tear open removes); 17 scripts
  pass at normal, 2x and no anti-aliasing; idle 0 ticks. Screenshot
  `docs/screenshots/m2-10-clean-up.png` (a top edge merged at its centre).
- Moved the 2026-11-18 entries to `LOG-archive.md`.

### 2026-11-21 — v0.2.5 agreed
- User on the proposal: merge with both options (centre / first selected); collapse,
  weld, dissolve, fill, flips, triangulate / join yes; split renamed **Detach**; Delete
  dissolves in point and edge mode, and also in face mode (asked: selected neighbouring
  faces join, one face alone shrinks to a point), with a separate **Tear open** for real
  holes; Double-sided on/off yes.

### 2026-11-21 — v0.2.4 "Cut and divide"
- `core/divide_ops`: `knife` (each two points cut across the face they share, that face
  alone: `cutShape` gained `wholeSurface = false`), `loopRing` (forward round the quads
  through opposite edges, then backward; closed or ending at triangles / an open edge)
  and `loopCut` (N cuts, one at a typed position or several spread evenly; the faces at
  an open ring's ends get the points), `subdivide` (quads N x M bilinear, triangles N x N;
  edge points shared, a clash between neighbours refused), `divideEdges`, `poke`,
  `connectPoints`, `slideTarget`; new points always go into the faces next to them.
- Found by a random test (loop cuts, subdivides, divides, pokes and knife steps one
  after another on a box, a cylinder and a sphere): points put on an edge are rounded
  up to 0.001 cm off it, which the angle-only "straight corner" test missed, so a sliver
  ear was cut and the last triangle dropped (open model). Corners are now straight when
  the point lies within 0.002 cm of the line; 1200 rounds clean.
- UI: `CutTool` (Shift+X, button; knife with point / midpoint / edge snaps; loop cut ring
  shown on hover, picked by a click, cuts and position typed), Vertex tool Alt+drag slide
  (edge chosen by screen direction, grid steps along it), Mesh menu Subdivide / Divide
  edges / Poke windows (operation windows with a red "Can't" line), Connect two points.
- Tests: core 1073 checks; new UI script `a2_cut`; sixteen scripts at 1×, 2× and without
  anti-aliasing; idle 0 ticks.
- Screenshot `m2-09-cut.png`. Version 0.2.4, README, Help → Controls, DEV, notes.

### 2026-11-21 — v0.2.4 agreed
- Proposal (knife, loop cut, slide with Alt in the Vertex tool, Mesh menu subdivide /
  divide edges / poke / connect): "yes to all".

### 2026-11-20 — v0.2.3 confirmed
- User: "seems like it is done". Next session: plan v0.2.4 "Cut and divide" click by click.

### 2026-11-20 — v0.2.3 "Outlines and holes"
- **Holes:** `throughDistance` (a drawn shape whose outline edges are all shared with
  faces in its plane or at right angles; the nearest parallel face facing the other
  way whose surface holds the projected outline) and `pushThrough` (the outline cut
  into the far surface, walls for edges inside the face, and for edges on its border,
  e.g. a doorway's sill, `openSide` takes the strip out of the face at right angles).
  Push/pull stops at that distance and makes the hole ("Through to the other side").
- **Outlines:** `makeOutline` (outline and holes: checked for crossings, holes joined by
  bridges, split into 3- and 4-corner faces facing the viewer; double-sided option).
  Draw's fourth choice, 2D views only: clicks on the view's plane through the origin,
  grid / point snapping, axis lock, first point closes, outlines inside are holes.
- **Surfaces (found while testing in the app):** after one cut a face is several
  pieces, and a new shape had to fit inside one piece (a doorway next to a window was
  refused). `cutShape` now works on the flat, joined group of faces (`surfaceOf`: its
  outline and holes): a shape inside becomes a hole of the surface filled by its own
  faces (rebuilt with bridges); a line or a shape touching the outline splits the
  surface, holes going with the side they lie in. Also found in the app: rectangles
  took the clicked piece's first edge as their direction (skewed on a cut wall); now
  world X (or Y) on faces along an axis, else the face's longest edge. And Push/pull in
  2D picked the tallest piece on a line; now the selected (just drawn) shape wins.
- Tests: core 1027 checks (window, circle through a slab, doorway, corner doorway,
  300 random shapes pushed through a wall, one wall with many holes one after another,
  outlines with 0-2 holes, refusals, 300 random outlines with holes; the earlier 900
  random cuts). New UI script `a2_holes` (window, doorway, outline with a hole pulled
  up). Fifteen scripts at 1×, 2× and without anti-aliasing; idle 0 ticks.
- Screenshot `m2-08-holes.png`. Version 0.2.3, README, Help → Controls, DEV, notes.

### 2026-11-20 — v0.2.3 agreed
- Repo re-checked after the audit merge (build folders gone, release rule, export pair,
  view fallback, hashes); reported to the user. v0.2.3 proposal (holes by pushing
  through, Outline with holes, double-sided): "yep to all".

### 2026-11-20 — Audit fixes (merged into main by the user's go-ahead, no version release)
- **Releases shipped later code** (twice: v0.0.0 got a later build fix, v0.1.0 later
  docs): any run on main published every notes file without a release, with its own
  build. Now a version release is tagged at the commit that added its notes and only
  published from a build whose code (outside `docs/`) matches that commit; otherwise
  the run warns. The `--latest` mark only goes to the newest version (counting notes on
  main, so a late re-run of an old build doesn't take it). Checked against
  the real history: the new rule holds back v0.0.0 and tags v0.1.0 at its notes commit.
- **Build folders removed** from the repo: `build-core/` (61 MB) and `build-dbg/`
  (42 MB), committed since alpha 1, with workspace paths and a Linux version string.
  `.gitignore` covers `build-*/`. They are still in history (parked in PLAN.md).
- **glTF export pair:** the `.bin` was renamed into place before the `.gltf`; if the
  second rename failed, an old `.gltf` was left next to a new `.bin`. Now
  `commitExportFiles` moves the old files aside first and puts them back on any
  failure. Probe: with the old loop the new test fails (2 checks: old `.bin` gone);
  with the fix it passes. Only plain files are replaced: a folder with the export's name
  is refused with an error and left alone (first version moved it aside; found by hand).
- **Views without anti-aliasing as a fallback:** if the multisampled view buffer can't
  be made (GPU memory, huge view), the view draws straight into its texture with its
  own depth buffer instead of staying blank. `MAZZ_VIEW_SAMPLES=0` forces this path; CI
  runs all UI scripts that way too. Screenshot: `m2-07-views-without-antialiasing.png`.
- **Dependency fingerprints:** GLFW, Dear ImGui and stb downloads are checked against
  SHA-256 in `cmake/Deps.cmake`. The hashes are of the archives as GitHub serves them
  (taken from CI's download; GitHub compresses differently from a local `git archive`,
  so they can't be made here). GLFW's matches the hash Homebrew publishes for 3.4.
- PLAN.md status rewritten (it still said "redesign milestone 1" above v0.2.x);
  HANDOFF "where things stand" and mistakes updated; DEV.md: fingerprints,
  `MAZZ_VIEW_SAMPLES`, release rules.
- Tests: core 976 checks; UI scripts at 1×, 2×, without anti-aliasing and both together
  pass; idle check control, 1×, 2× and without anti-aliasing: 0 ticks. By hand: exports
  over existing files (.gltf twice, .glb, .obj) read back with an outside glTF/OBJ
  reader, `.bin` size matches the `.gltf`, no leftovers. The release script was dry-run
  with a fake `gh` against real commits (v0.0.0 held back, v0.1.0 tagged at its notes
  commit, `--latest` only on the newest).

### 2026-11-20 — v0.2.2 "Draw on faces"
- `core/draw_ops`: `faceFrame`, `splitPolygon` (convex: bands across; otherwise ear
  clipping, best-shaped ear first, then pairs of triangles joined into convex quads;
  points in a straight line allowed), `placeOnFace`, `cutShape` (closed shape inside:
  the face becomes a frame joined by two bridges; closed shape touching the border in
  one stretch, or an open line: split along the path; neighbours get the new border
  points, all of a neighbour's edges at once), `rectanglePoints`, `circlePoints`. Cuts
  leaving a face under 0.001 cm² are refused. `core/model_ops`: `pushKind` and
  `pushPullFaces` for several flat faces together.
- Bugs found by a random test (shapes and lines on boxes and a cylinder, 300 rounds per
  model, 2000 tried once): a neighbour bordering the cut face along two edges got only
  one new point (open surface); an ear whose cut ran through another point (in a line,
  off by the 0.001 rounding) left a gap; the last triangle of an ear clip could be
  turned over and was dropped. All fixed; every round now leaves a closed model with
  good faces, or refuses and changes nothing.
- `DrawTool` (Shift+D, button): rectangle (two clicks or press-drag-release; typed
  width / height; corners dragged), circle (centre, radius; typed radius, segments),
  line (clicks, Return, Backspace, double-click finishes); trial cut on a copy for the
  red "doesn't fit" and the reason; snapping (points on the plane, midpoints, edges
  with grid steps along axis edges, centre, grid in world or face frame); direction
  lock by screen distance to the axis line (8 px), Shift keeps it; markers only for
  non-grid snaps; no face highlight (user). Push/pull on a selected drawn shape pushes
  all of its flat, joined, selected faces.
- Tests: core 937 checks; new UI script `a2_draw` (rectangle typed, pushed up; too big
  refused; door from the floor; circle 12 segments; line with Z lock); fourteen
  scripts at 1× and 2×; idle 0 ticks.
- Screenshot `m2-06-draw.png`. Version 0.2.2, README, Help → Controls, DEV, notes.

### 2026-11-20 — v0.2.1 confirmed; planning v0.2.2 "Draw on faces"
- User: v0.2.1 all good. Proposal for v0.2.2 sent (click by click). Answers: no hover
  highlight; rectangles, circles, lines as proposed; snapping hints yes; direction lock
  yes (after an explanation); push/pull "snap to a point's height" dropped. The user
  found the drawing plan confusing and said to go ahead (revert if needed). Split:
  v0.2.2 "Draw on faces", v0.2.3 "Outlines and holes"; later releases move up one.

### 2026-11-20 — v0.2.1 "Edges and points"
- Agreed ("perfect"): Push/pull in edge and point mode as an along-normal dragger, and
  Inset not starting again on its result; shipped as a small v0.2.1, so the later
  releases move up by one (Draw on faces is now v0.2.2 ... Bevel v0.2.5).
- `core/model_ops`: `surfaceNormal` (edge: faces using both its points; point: faces
  using it; the unit average), `pushPoints` (moves the points; refused when a face
  around vanishes or turns over).
- `PushPullTool`: targets a face, an edge or a point by the selection mode; edges and
  points are picked when their normal isn't within ~25° of the view direction (stacked
  points in 2D: the nearest); highlight and arrow for edges and points.
- `SelectionOpTool`: `againAfterApply()` (Extrude yes, Inset no); Inset says "Done.
  Click faces to inset again."
- Tests: core 876 checks (edge normal, edge pushed out / too far, corner normal, open
  edge of a flat face); new UI script `a2_push_edges`, `a2_extrude` checks that a new
  width after Return waits for a click; thirteen scripts at 1× and 2×; idle 0 ticks.
- Screenshot `m2-05-push-point.png`. Version 0.2.1, README, Help → Controls, release
  notes.

### 2026-11-20 — after v0.2.0: inset count checked; push/pull for edges and points agreed
- User: inset seems to make more than 4 faces. Tested in the app: a lone 80 cm square,
  inset 10 → 5 faces, 8 points (4 new faces), as designed. Possible sources sent to
  the user (the status bar counts triangles, 2 per quad; changing a setting after
  Return starts a new inset on the inner face).
- Agreed: Push/pull in edge and point mode drags along the surface normal (no new
  faces). Extruding inner edges / points not planned. Recorded in ARCHITECTURE §6.

### 2026-11-20 — v0.2.0 "Push and pull" (steps A–E)
- Agreed with the user: milestone 2 in five releases (v0.2.0–v0.2.4, PLAN), separate
  into group and smoothing of extruded sides moved to milestone 3; push/pull, extrude
  and inset click by click (ARCHITECTURE §6); Inset its own tool (Shift+I); every tool
  a button in the Tools panel, one-step commands in the menus only. The tools are in
  the Tools menu (a separate "Modelling" menu, mentioned in the proposal, would have
  repeated it).
- `core/model_ops`: push/pull (stretch when every face touching the face's points
  lies at right angles to it; else new side faces; Cmd = new faces; a face with an
  open side becomes a solid), extrude faces (region / each; along normals with corners
  moved so every face moves exactly the distance; straight; keep original), extrude
  open edges (the strip continues the surface), inset (region / each; width; depth =
  the inner faces extruded in with straight walls, leaving a flat frame), `isClosed`,
  `signedVolume`. Refused when faces would turn over or vanish (too far, too wide,
  straight along the face's own plane). Fixed on the way: `flipped()` changed which
  diagonal a quad was split along (now kept).
- Tools: `PushPullTool` (hover lights the face; 2D views pick faces seen edge-on within
  6 px; drag along the normal via the closest point between the mouse ray and the
  normal line; grid steps; release keeps the preview; Return / Esc; another face applies
  the pending push first; red when refused), `SelectionOpTool` (shared by Extrude and
  Inset: preview while the selection suits, Return applies and selects the result, Esc
  drops it until the selection or a setting changes; selecting with `PickTool`, the
  Select tool without moving), `ExtrudeTool` (arrow from the selection's centre along
  its normal or the axis, dragged with grid steps), `InsetTool`. Tool interface gains
  `update()` (each frame) and `activate()`. Selection commands (Select all / none /
  grow / shrink / loop / ring) drop a preview first.
- Tests: core 865 checks (push/pull stretch / new faces / flat shape / too far, extrude
  region / each / corner / straight / keep original / edges, inset / depth / too wide
  / triangle / region / each, flipped diagonal; every closed result checked closed with
  the right volume). UI scripts `a2_pushpull`, `a2_extrude`; twelve scripts pass at 1×
  and 2×; idle 0 ticks at 1× and 2× with an armed Extrude preview added to the scene.
- Screenshots `m2-01-pushpull.png`, `m2-02-push-too-far.png`, `m2-03-inset.png`,
  `m2-04-extrude.png`. Version 0.2.0, README, Help → Controls, DEV script commands,
  release notes.

### 2026-11-20 — v0.1.4 choices confirmed; milestone 2 planning starts
- User: both v0.1.4 choices are fine (reference images stay on New / Open; Export again
  only while the model is open). Parked questions removed from PLAN. Planning of
  milestone 2 begins (proposal sent: releases and the first release click by click).

### 2026-11-20 — v0.1.4 "Out of the app" (steps A–E)
- `io/export`: `.glb`, `.gltf` + `.bin`, `.obj` + `.mtl` through cgltf 1.15 (vendored in
  `third_party/cgltf`, the version Scalpellino uses). Z-up cm → Y-up m as
  (x, z, −y) / 100, flat normals, quads split by their stored diagonal (as drawn), one
  grey material (0.75, exact in the file), `asset.extras` `{"sourceUnits":"cm",
  "sourceUp":"Z"}`. OBJ numbers written exactly from the clean cm values (62.5 cm →
  `0.625`). Written beside the target and renamed. Tests read them back with cgltf
  (validate, counts, bounds, every point, unit normals, winding) and parse the OBJ; a
  deliberately mirrored conversion made 9 checks fail, so they catch it. The Khronos
  glTF validator finds 0 errors and 0 warnings in a `.glb` and a `.gltf` torus (only
  the hint that bufferView targets are missing: cgltf leaves them out on purpose).
- File → Export (three formats, the name of the model or the last export suggested),
  Export again on Cmd+Shift+E (asks the first time; forgotten on New / Open), status
  bar note "Exported to …" until the model changes.
- Reference images (`core/ref_image`, `app/ref_images.cpp`, `render/image_draw`):
  "Image..." in each 2D view's header; one per direction, shared, none in 3D, not
  saved; fills the visible view (5 % margin), then width / rotation / transparency /
  behind or in front in its window. While the window is open the views facing its way
  move it (drag) and resize it (corner, opposite one fixed, proportions kept); when
  snapping, moves go in grid steps and the width is whole steps (a first version
  without that landed 0.13 cm off at the script's zoom, from pixel rounding). Kept on
  New / Open for now (parked question).
- Scale figure (`core/figure`): Scalpellino's boxes, 175 cm, light blue outlines in 2D,
  see-through in 3D (drawn after the model without writing depth); stands at
  x = floor(min(0, model's min x) − 25 − half its depth), y = 0, on the ground.
- Script commands `export`, `testimage`, `refload`, `refset`, `refwindow`, `refremove`,
  `expect file | filehas | refimage | ref | figure`. New UI scripts `a1_export`,
  `a1_reference`; the idle scene now has two reference images (behind and in front)
  and the figure.
- Help → Controls and README rows, About lists cgltf, THIRD_PARTY, version 0.1.4,
  release notes. Screenshot `m1-10-reference-figure.png`.
- Tests: core 790 checks; ten UI scripts at 1× and 2×; idle 0 ticks at 1× and 2×
  (control 873 ticks).

### 2026-11-20 — v0.1.4 design agreed; steps planned (docs only)
- User tried v0.1.3 (Mesh menu found; works). Agreed for v0.1.4 ("yes to all"):
  File → Export with `.glb` / `.gltf` / `.obj`; export again on **Cmd+Shift+E** (Cmd+E
  was proposed first, but it is Frame selection, here and in Scalpellino); whole model;
  flat normals until smoothing groups; quads split by their diagonal; one grey
  material; `.obj` Y-up metres like glTF. Reference images: views facing the same way
  share one image, none in 3D, a new image fills the visible view then its width is
  typed. Scale figure as in Scalpellino (blocky, see-through, 175 cm, never exported),
  View → Scale figure without a shortcut, beside the origin on −X outside the model,
  not saved.
- ARCHITECTURE §5, §6 shortcut table, §7, §11 updated; PLAN: steps of v0.1.4 (A–E).

### 2026-11-20 — v0.1.3 "Typed values" (steps A–E)
- `core/ops`: transform about a pivot (scale, rotate X→Y→Z, move, along the
  selection's normal; mirrored results turn their faces round), replicate (per-copy
  scale / turn / offset, scaled offset, travelling / origin / typed pivot), duplicate,
  mirror in place, mirror clone across the world plane, flatten average / lowest /
  highest. Core tests (exact quarter turns, rows, rings, tapering, mirrored copies stay
  closed and outward, flatten).
- Operation windows (`ui/op_windows.cpp`): small windows that stay open over the app;
  each change re-runs the operation on a live edit (reverted first), so the views show
  it live; Return applies (one undo step), Esc / the close button / any tool switch,
  undo or file action cancels; tools don't take clicks while one is open. Transform
  (Cmd+T, also "Move points exactly" Cmd+Shift+T), Multi-Replicate (Cmd+Shift+D),
  Flatten. Cmd+D Duplicate, Mirror / Mirror clone X/Y/Z, Snap points to grid (Cmd+B).
- Found by the new script: after Esc, the Transform window brought back the cancelled
  values next time (relative values, so a trap): it now opens at zero, keeping the
  pivot choice. The window first opened over the 3D view (now top left): it now opens
  over the texture view.
- The Vertex tool works on points only (2/3/4 switch to Select).
- Script commands `opset`, `expect op`. New UI script `a1_typed`.
- Tests: core 667 checks; eight UI scripts at 1× and 2×; idle 0 ticks. Version 0.1.3,
  release notes, README. Screenshot `m1-09-transform.png`.

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

