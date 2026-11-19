# Mazzuola — Architecture & Design

A small, fast modeller for static low-poly 3D models: few polygons, small hand-painted
pixel textures, vertex colours. Early 3D games and tools are an inspiration, not a
target; nothing has to be period-accurate.
Its own tool, fitted to the user's workflow: models are made here and placed in
Scalpellino scenes (or opened in any glTF tool).

Personal tool. Not distributed. Lightweight; does what it does well.

---

## 1. Guiding rules

1. **Idle = zero work.** Redraw and recompute only on change (input, resize, an edit,
   a texture file changing on disk). No continuous frame loop.
2. **The model file is the truth.** GPU meshes, previews and exports are rebuilt from it.
3. **The core has no graphics.** Mesh maths, texture coordinates, pixel painting, file IO
   and undo live in a plain C++ library, tested headless on CI.
4. **Everything is undoable.** Every edit, including every brush and paint stroke, is a
   command.
5. **Static models only.** No skeletons, animation, rigging or physics.
6. **A look, not old hardware.** Low poly counts, small textures, nearest-pixel
   filtering by default, vertex colours. No emulated hardware glitches, no enforced limits.
7. **One window, mouse and keyboard, is the complete tool.** Extra screens and tablets
   are optional comforts; nothing depends on them.
8. **Made for one good model at a time.** Tools are chosen for shaping, texturing and
   painting a single object. Texturing is as important as modelling: the texture view
   is always one glance away.
9. **Feels like Scalpellino where it can.** Same shortcut patterns, menus, typed dialogs
   and Return/Esc habits, so moving between the two is effortless.
10. **Single-threaded.** Models are tiny.

## 2. Platform

- **macOS, Intel first**, Apple Silicon built on demand. Linux builds for CI tests.
- **Keyboard: Italian layout assumed.** Physical key positions where it matters; no
  shortcut needs an Option character. Cmd for app commands.
- Optional: a second display (including an iPad through macOS Sidecar) and a drawing
  tablet or Apple Pencil for the vertex brush and painting.

## 3. Privacy rules (for every contributor and every AI session)

- No machine details in the repo, commits or CI logs: no hostnames, usernames, home
  paths, OS versions, hardware.
- Commits authored as `Claude <noreply@anthropic.com>`. No links, no session URLs, no
  trailers.
- Personal settings (recent files, layout, palettes, window positions) live in the
  app's settings folder, never in the repo.
- Pasted logs are scrubbed before anything is committed.

## 4. Layers

```
┌──────────────────────────────────────────────┐
│ UI  (Dear ImGui, docking + multi-viewport)   │  menus, panels, texture view,
│                                              │  paint view, palette, problems
├──────────────────────────────────────────────┤
│ Tools                                        │  select, vertex, draw, push/pull,
│                                              │  extrude, cut, lathe, loft, brush,
│                                              │  mapping, paint, colour
├──────────────────────────────────────────────┤
│ Render (OpenGL core)         │ IO            │  model views, texture view │
│ Platform (GLFW, Cocoa)       │               │  .mazz, .glb/.gltf, .obj, PNG
├──────────────────────────────────────────────┤
│ Core  (no graphics, fully tested)            │  mesh, edit ops, texture coords,
│                                              │  images, painting, commands/undo
└──────────────────────────────────────────────┘
```

Each layer only talks to the one below it.

**Edit flow:** input → active tool builds a command → command changes the document and
marks what changed (mesh, a texture rectangle, vertex colours) → only that is re-uploaded
to the GPU → one redraw of the views that show it.

## 5. Core data model

- **Model**: one document. Vertices, faces, parts, smoothing groups, textures, pivot,
  mirror setting, palette.
- **Vertex**: a position (doubles in core, floats for rendering).
- **Face**: a triangle or a quad. Each face has:
  - its corner vertices,
  - one **texture** index (or none),
  - one **part** (or none),
  - one **smoothing group** (or none = flat),
  - **double-sided** on/off,
  - per corner: a **texture coordinate** in texture pixels and a **vertex colour**
    (RGB, default white),
  - for quads: which diagonal splits it into triangles (flippable).
  Texture coordinates and colours are per corner, so seams and hard colour edges are
  possible; tools keep corners at one vertex together ("welded") unless split.
  Tools that would leave a face with more than four corners split it into
  triangles/quads automatically, with a preview.
- **Smoothing group**: faces in the same group meeting at an edge look smooth there;
  different groups (or none) give a hard edge. Used for exported normals.
- **Part**: a named group of faces (lid, body, handle) with a visibility flag and its
  own **pivot** (e.g. a lid's hinge), usable as the rotate/scale centre and exported as
  the part's node origin.
- **Texture**: a PNG file next to the model (relative path), power-of-two sides from 16
  to 256 suggested, with a **filtering** setting: nearest (default) or smooth. A model
  may use several textures. Fully transparent pixels are cut
  out (for leaves, fences, hair).
- **Reference images**: an image pinned to a view direction (front, side, top…) with
  position, rotation, scale, a transparency slider and a front/back switch (drawn in
  front of or behind the model). Editor-only, saved in the model file.
- **Units / axes**: 1 unit = 1 cm, Z-up (matches Scalpellino, so models place at their
  real size). A 175 cm human figure can be shown for scale (same as Scalpellino's).
- **Pivot**: the model's origin, becomes the placement point in Scalpellino.
- **Mirror mode**: optional symmetry plane (X by default). Edits, brush strokes and soft
  selection on one side are repeated on the other; vertices on the plane stay on it.
- **Settings per model**: texel target (pixels per cm, optional; 0.5 suggested for
  room-sized objects, more for small ones), palette.

## 6. Interaction design

### Window layout (default)
```
┌──────────────┬──────────────┬──────────────────┬─────────┐
│     Top      │    Front     │                  │  Tools  │
│              │              │   Texture view   │  ─────  │
├──────────────┼──────────────┤  (texture sheet  │  Props  │
│     Side     │      3D      │   + pixel grid)  │  ─────  │
│              │              │                  │ Parts / │
│              │              │                  │ Palette │
└──────────────┴──────────────┴──────────────────┴─────────┘
  status bar: tool · selection · cursor position · vertices / triangles
```
- **Four views** by default: top, front, side (orthographic) and 3D. Any view can be
  maximised and switched to another direction (front/back/left/right/top/bottom).
  Splitters resize; double-click restores.
- **3D camera**: right mouse drag orbits around the origin or the selection (two buttons
  in the view's header), middle mouse (or Space + left mouse) pans, the wheel zooms.
  **Fly** (Z over the 3D view, or the header button): mouse looks, W A S D move, Q/E
  down/up, Shift fast; Z or Esc stops.
- **2D views**: wheel zooms toward the cursor, middle mouse (or Space + left mouse) pans.
- **Texture view**: always visible next to the views (§6 Texture coordinates).
- **Paint window**: a second app window with its own 3D view and camera, pen-sized tools
  and palette (§8). Optional; everything it does can also be done in the main window.
- Display modes per view: wireframe, flat, vertex colours only, texture only,
  texture × vertex colours (normal), texel check; wireframe overlay; show back faces.
  Each texture is shown with its own filtering (nearest by default). The texture view
  always shows exact pixels.
- **Counter**: vertices and triangles (total, selected, hidden).

### General rules
- Menus: File, Edit, Select, Tools, **Create** (everything that makes new geometry:
  primitives, rock, lathe, loft, image card, tree), Mesh (editing commands), Texture,
  Paint, View, Help. Features of later milestones are listed, disabled, with the
  milestone that brings them.
- Every shortcut also in the menus with typed dialogs; Help → Controls lists them.
- Creation commits on **Return**, cancels on **Esc**, never on mouse release.
- Operations with settings (extrude, bevel, divide…) show a live preview and a panel of
  typed values; Return applies, Esc cancels.
- No wheel parameter adjustment; the wheel zooms views only.
- **Look: light theme with a red accent** (Scalpellino is dark with blue, so the two
  are told apart at a glance). The accent marks the active tool, focused fields,
  buttons, tabs and selected list rows. Views keep the usual axis colours (X red,
  Y green, Z blue); selection in the views is drawn in pink so it never reads as the
  X axis or the UI accent. Model views use a light grey background, the texture view a checkerboard
  behind transparent pixels.
- UI font Roboto. Grid halve/double on the two keys right of P; grid and snap toggles.
  Grid steps match Scalpellino's (in cm), plus steps below 1 cm (millimetres) for small
  objects.

### Shortcuts (proposed, Scalpellino patterns; confirmed in M0)

| Action | Keys |
|---|---|
| Tools: Select / Vertex / Primitives / Draw / Push-pull / Extrude / Cut / Lathe-loft / Vertex brush / Mapping / Paint / Vertex colour | **Shift+S** / **Shift+V** / **Shift+B** / **Shift+D** / **Shift+F** / **Shift+E** / **Shift+X** / **Shift+C** / **Shift+G** / **Shift+A** / **Shift+P** / **Shift+O** |
| Selection mode: vertex / edge / face / piece | **1** / **2** / **3** / **4** |
| Select all / none | **Cmd+A** / **Esc** |
| Exact move / rotate / scale | **Cmd+T** |
| Move points exactly (vertex tool) | **Cmd+Shift+T** |
| Snap points to grid | **Cmd+B** |
| Duplicate / clone | **Cmd+D** / Shift+drag |
| Apply current texture | **Shift+T** |
| Hide selected / hide others / show all | **H** / **Shift+H** / **Cmd+U** |
| Frame selection / back to origin | **Cmd+E** / **Cmd+0** |
| Problems list | **Cmd+Shift+O** |
| Fly mode (3D view) | **Z** |
| Smaller / larger grid | **è** / **+** |
| Show grid / snap to grid | **Shift+R** / **Shift+W** |
| Undo / redo | **Cmd+Z** / **Cmd+Shift+Z** |
| New / open / save / save as | **Cmd+N** / **Cmd+O** / **Cmd+S** / **Cmd+Shift+S** |

### Selection
- Vertices, edges, faces, or connected pieces (1–4). Click, Cmd+click adds/removes,
  box select in any view, select similar (texture, smoothing group, direction),
  grow/shrink, edge loop / ring, select part. "Whole faces only" option for box select.

### Vertex tool (Shift+V)
- Move selected points by dragging, with axis arrows in 2D views; rotate and scale with
  handles; typed values (Cmd+Shift+T).
- **Soft selection** (on when several points are selected and the toggle is on): typed
  radius and curve (smooth, linear, sharp, constant), shown as a colour gradient; the
  surroundings follow moves, rotations and scales with falloff. Distance measured
  straight or over the surface (a setting).

### Shaping tools
- **Transform**: move / rotate / scale with handles in every view; Transform dialog
  (Cmd+T) with pivot (bounds centre, average, model pivot, part pivot, typed point) and
  angle snap. **Clone**: N copies at a typed distance or angle (in a row, or in a circle
  around the pivot), spacing given per copy or as a total. Each copy can also add a
  **rotation per copy** (X/Y/Z degrees, about the chosen pivot) and a **scale per copy**
  (X/Y/Z %, multiplied each step), with an option to **scale the offset** too: spiral
  stairs, tapering towers, twisted columns. Copies stay in the original's part.
- **Mirror** (along X/Y/Z) and **mirror clone**. **Flatten X/Y/Z**: line selected points
  up on one axis. Move along the selection's normal.
- **Primitives** (Shift+B): plane (rows, columns, diagonal direction), box (stacks per
  side, optional rounded edges with segments), cylinder (sides, stacks, cap segments,
  rounded edges), cone, tube (inner/outer sides, length), torus, sphere (sides and
  stacks) and geosphere (iterations). Drag in a view, adjust, Return creates.
- **Rock** (Create menu, generator): a random low-poly rock filling a dragged or typed
  box. Faces (12–60, target triangle count), roughness, stretch X/Y/Z, flat bottom
  (with height), join into quads, seed. Built from the hull of jittered points on an
  ellipsoid, then pushed in/out along normals without self-intersection. One smoothing
  group or auto smooth; box-projected texture coordinates at the texel target; pivot at
  the bottom centre.
- **Draw** (Shift+D):
  - on a face: lines, rectangles and circles drawn onto a face split it into new faces;
  - in a view: a polygon outline point by point (click the first point to close),
    filled automatically; holes allowed; option to make it double-sided.
- **Push/pull** (Shift+F): click a face (or a drawn shape on a face), drag in or out
  along its normal, or type the distance; pushing all the way through a solid cuts a
  hole.
- **Inference snapping** while drawing, pushing and moving: endpoints, midpoints, edges,
  face centres, axis directions, parallel and perpendicular to edges; a coloured hint
  shows the active snap; holding Shift locks the current direction.
- **Extrude** (Shift+E): faces as one region or each on its own, along normals or in a
  straight direction, typed distance; edges too. **Keep original** leaves a reversed copy
  behind (turns a flat shape into a solid). New side faces get smoothing groups by angle
  or as one group. **Inset** (region / each face) and **bevel** (edges or corners, typed
  width, 1–3 segments) share the tool.
- **Bridge** (join two open edge loops), **shell** (give an open surface a typed
  thickness: inner wall plus rim).
- **Divide**: **subdivide** selected faces (2×2 or N×M; plain or smoothed), **divide
  edges** into N, **poke** (centre point, typed height), **connect** (edge between two
  selected points across a face).
- **Cut** (Shift+X): knife (click points on edges), **loop cut** (N cuts around a quad
  ring, typed position).
- **Slide** a point along an edge or an edge loop along its neighbours.
- **Merge** (to centre / to last selected), **collapse edge**, **weld by distance**,
  **dissolve**, **fill** (face from 3 or 4 points), **split** (detach), **separate into
  part**, **delete**, **flip faces**, **flip quad diagonal**, **triangulate / join into
  quads**, **double-sided on/off**.
- **Lathe** (Shift+C): draw a profile in a 2D view, move the axis (vertical or
  horizontal), typed sides and angle (less than 360 gives a slice, capped or open),
  smoothing groups per profile segment; Return creates.
- **Loft** (Shift+C): a path drawn in a 2D view and a cross-section (circle by default
  with typed sides, or edited in a small section editor: move points, mirror, scale,
  rotate). Rounded path corners with typed segments; section scale and twist along the
  path; closed or open path; capped ends optional.
- **Mirror mode** toggle and **Apply mirror**.
- **Pivots**: place the model's pivot or the selected part's pivot by click (snaps to
  points, edge midpoints, face centres, bottom centre of the bounds) or typed position.
- **Parts**: assign selected faces to a part; hide, show, isolate, select, rename;
  **isolate copy** (a part that shares no points with others). Exported as separate
  nodes (origin at the part's pivot) or merged (an export option).
- **Smoothing groups**: new group from selection, remove smoothing, **auto smooth**
  (typed angle: sharper edges become hard). Primitives get sensible groups when made.

### Generators and random tools
Common to rock, tree, jitter, scatter and generate texture:
- A **seed** field plus a **New seed** button: the same seed and values always give the
  same result.
- Values are remembered between uses during the session (like the Transform dialog).
- Live preview, **Return** applies, **Esc** cancels, one undo step, menu access only
  (shortcuts can be added later on request).
- Generators never produce problems (zero-area faces, duplicate points, faces over four
  corners, inconsistent winding); tests check this over many seeds.

- **Jitter points** (Mesh menu): move the selected points by random amounts. Amount
  (largest move), direction (any / along normal / X / Y / Z), keep on grid, keep open
  edges still, seed. Stacked points move together (welded corners stay welded); hidden
  parts untouched; mirror mode mirrors the jitter and keeps plane points on the plane;
  texture coordinates stay as they are.
- **Scatter on faces** (Mesh menu): select a piece (a leaf card, a rivet), choose the
  command, pick target faces in any view (click, Shift+click). Copies, spacing (evenly by
  area / random), align to surface, random spin and spin step around the normal, size
  range, sink, avoid overlaps (reports "placed 8 of 10"), seed. The piece's pivot (or
  bottom centre) sits on the surface; mirror mode places on one side and mirrors; copies
  join the source's part.
- **Image card** (Create menu): a quad sized from a PNG, with its transparency. Size from
  the texel target or a typed width (height follows the image), facing (front / side /
  top / current view), style (single / crossed pair at 90° / crossed three at 60°), base
  (bottom / centre), double-sided, trim to the image's opaque area. Click to place
  (snaps). Texture coordinates fill the image, pixel-snapped; the image becomes one of
  the model's textures and is watched like the others.
- **Tree** (Create menu, generator): trunk and branches from tapered lofts, foliage as
  cards, crossed cards or clumps.
  - Trunk: height, base/top radius, sides (3–8), bend (degrees + direction).
  - Branches: levels (0–3), branches per level, start height on parent, angle from
    parent, spin between branches (default 137.5°), length/radius falloff per level,
    segments per branch, randomness, seed.
  - Foliage: style (none / cards / crossed cards / clumps), where (branch tips / along
    the last level), size, count per tip, texture (alpha cut-out on for cards), clump
    faces (clumps are softer rocks).
  - Click a point for the base; branches overlap their parent instead of being welded
    (fewer faces). Output: two parts, **trunk** (pivot at the base, one smoothing group
    per branch, cylindrical texture coordinates along each branch with the seam at the
    back) and **foliage** (double-sided cards, texture coordinates filling the texture).
    The counter shows the preview's counts.

### Vertex brush (Shift+G)
- Drag in any view: the points within the brush radius move with the pointer, the ones
  near the centre fully and the ones near the edge less (typed radius and falloff curve;
  a circle on the surface shows the radius). Optional lock to an axis or the normal.
- Only existing points move. Affects the selection if one exists, otherwise everything
  visible; hidden parts are never touched. Mirror mode applies. Pen pressure (when a
  tablet is used) scales the reach.
- One stroke = one undo step; Esc during a stroke cancels it.

### Problems list (Cmd+Shift+O)
- Lists zero-area faces, duplicate faces, duplicate points, unintended open edges, faces
  without a texture, flipped neighbours.
- Each entry: jump to it (camera frames it), and **Fix** (the repair for that kind of
  problem, named on the button: "Weld", "Delete face", "Flip", …).
  **Fix all** applies every fix as one undo step. Next/Prev step through the list.

### Texture coordinates (mapping tool, texture view)
- **Mapping tool** (Shift+A): give selected faces texture coordinates by **planar**,
  **box**, **cylindrical** or **spherical** projection, oriented by a view direction or by
  clicking a face in 3D (along its normal). **Auto-unwrap**: **unfold** (all selected faces
  laid flat as one piece, with a "try another start face" button) or **flatten by angle**
  (split into flat pieces wherever faces bend more than a typed angle).
- **Texture view**: the texture sheet at pixel scale with a pixel grid, showing the
  selected faces (or all). Select corners, edges, faces or pieces (1–4), in the texture
  view or the model views; selection is linked both ways.
  - Move, rotate (90° steps or typed, or by dragging a corner), scale, mirror; single
    points dragged directly; snapping to texture pixels (half-pixel optional).
  - **Weld** joins texture corners of the same model point (closes a seam); **unweld**
    splits them; **stitch** moves a piece so an edge meets its neighbour on the sheet;
    **regroup** makes the selection its own piece.
  - Overlapping and mirrored pieces are allowed on purpose; areas shared by several faces
    are highlighted.
  - Texture choice per selection; **tile texture** display option.
- Shaping tools keep texture coordinates where they can (extrude stretches from the
  edge they came from; divide interpolates).
- **Texel check** display mode (pixels per cm as colour, or a checker texture) and
  **even out** (rescale selected pieces to the texel target).
- Moving pieces that already have paint under them shows a warning: paint doesn't move
  with them. Layout first, then paint.

### Texture painting in Mazzuola (Shift+P)
- Pixel tools: pencil (1–4 px square), line, rectangle, fill (within the face or the
  whole sheet), colour picker, eraser (to a chosen colour or fully transparent).
- **Palette** saved with the model; **saved palettes** in settings to reuse across models.
- Paint on the texture view or **directly on the model** in any 3D view: the pen
  position becomes a face, then a point on that face, then a texture pixel. Strokes fill
  in between samples, so fast strokes stay continuous across face edges. Shared texture
  areas show the stroke on every face that uses them.
- One stroke = one undo step (the changed rectangle, before/after).

### Texture painting in another app
- **Export layout image**: a transparent PNG with the face outlines at ×1/×2/×4/×8, to
  paint over in another app.
- Texture files are watched: when Mazzuola's window becomes active and a texture changed
  on disk, it reloads. With unsaved paint on that texture, it asks which to keep.
- **Import scaled image**: an image painted at ×N is reduced to the texture size with
  nearest-pixel sampling.

### Generate texture (Texture menu)
- A starting pixel texture to paint over. Size (16–256 per side), pattern (noise /
  clouds / bricks / planks / wood grain / stone tiles / gradient) with its own values
  (e.g. bricks: rows, columns, mortar width, offset), colours (from the model's palette
  or two typed colours), colour count (2–16), dither (none / ordered 2×2 / 4×4),
  tileable, seed.
- Preview at pixel scale, plus tiled 3×3 when tileable. Return creates a new PNG next to
  the model (asks for a name) or replaces the current texture (one undo step). Every
  pixel is one of the chosen colours; never filtered.

### Vertex colours (Shift+O)
- Separate from the texture, multiplied over it.
- Soft round colour brush (typed radius and strength; pen pressure scales strength),
  fill selected, picker, **split colours** for hard edges.

## 7. File formats

- **Native `.mazz`**: plain text, one file per model, readable and diffable. Textures are
  separate PNG files referenced by relative path. Sketch (final syntax at M1):

  ```
  mazzuola 1
  units cm
  pivot 0 0 0
  texel 0.5
  texture 0 "lamp.png" 64 64 filter=nearest
  part 0 "body" pivot=0,0,0
  part 1 "shade" pivot=0,0,100
  sgroup 1 "round"
  v 0 0 0
  v 40 0 0
  ...
  f 0 1 2 3 tex=0 part=0 sg=1 two=0 diag=0
    uv 0 0 / 16 0 / 16 16 / 0 16
    col ffffff ffffff c0c0c0 c0c0c0
  ref front "lamp-front.png" x=-80 y=0 rot=0 scale=0.5 alpha=0.4 layer=back
  palette 1a1c2c 5d275d b13e53 ef7d57
  ```

- **Export `.glb` / `.gltf`**: positions, normals (from smoothing groups), texture
  coordinates, vertex colours (`COLOR_0`), textures (embedded in `.glb`, beside `.gltf`)
  with samplers matching each texture's filtering, alpha cut-out, double-sided materials, optional
  `KHR_materials_unlit`, parts as separate nodes (origin at the part pivot) or merged.
  Converted to Y-up and meters (÷ 100); units (cm) and pivot in `extras` so Scalpellino
  converts back exactly.
- **Export `.obj` + `.mtl`**: positions, normals, texture coordinates, materials with
  texture paths, parts as groups (vertex colours as the common unofficial extension).
- **Images**: PNG in and out. TGA/BMP/JPG accepted as reference images.

## 8. Paint window and tablets (optional)

- The **paint window** is a second, real app window (View → Paint window), not a panel
  pulled out of the main one. Same program, so model, textures and undo are shared and
  every stroke shows everywhere at once. Put it on any display: a second monitor, or an
  iPad through macOS Sidecar with the Apple Pencil (nothing on the iPad is ours).
  Implemented as its own GLFW window with its own ImGui context and its own GL objects
  (VAOs and framebuffers aren't shared between OpenGL contexts; textures are).
- Panels can't be pulled out of the main window (ImGui multi-viewport is off). Tried in
  milestone 0: on macOS it was fragile (a crash when a display went away) and pulled-out
  panels didn't take the Pencil on Sidecar.
- **Sidecar needs graphics switching.** On a Mac with two GPUs, OpenGL windows don't
  update on a Sidecar display unless the context allows offline renderers
  (`GLFW_COCOA_GRAPHICS_SWITCHING`) and Info.plist has
  `NSSupportsAutomaticGraphicsSwitching`. Both are set; keep them.
- **Pen taps**: a pen tap comes without a pointer move first, and GLFW only updates the
  position on moves. `platform/pen_mac.mm` (a Cocoa event monitor) gives ImGui the real
  position before each click, marks pen events as pen input, and reads **pressure and
  tilt** (used for the vertex brush's reach and vertex colour strength; pixel tools
  ignore pressure; without a tablet the typed values are used).
- In the paint window a plain drag turns the view (a pen has no right button); two-finger
  scroll zooms. Buttons are pen-sized.
- **Pencil double-tap** (when available): switch between the current tool and the
  picker or eraser (a setting).
- Its position is remembered; if its display is gone, macOS moves it to another one.
- Windowing: GLFW + OpenGL + ImGui's own backends.

## 9. Rendering

- OpenGL core profile (4.1 on macOS). A small shader set: textured × vertex colour with
  alpha cut-out, flat, texel check, soft-selection gradient, lines, grid, inference
  hints. Picking (faces, points, edges, box select) is done on the CPU in `core/pick`
  against the same matrices the views draw with; points hidden behind faces are skipped
  in 3D. A GPU face-ID buffer stays an option for painting (M6) if the CPU path is too
  slow there. Double-sided faces drawn without culling.
- Views draw only when what they show changes (a scene version number), into their own
  offscreen images. Selection is pink; what a creation tool is about to make (the
  preview, before Return) is cyan.
- No lighting effects beyond the display modes. No post-processing.

## 10. Undo

Each command stores before/after copies of what it touches: the mesh, the changed
rectangle of a texture, or the changed vertex colours.

## 11. Dependencies

Fetched by CMake, versions pinned:
- GLFW: windows, input
- Dear ImGui (docking branch) with its GLFW + OpenGL3 backends
- cgltf: `.glb`/`.gltf` writing (and reading in tests); added in milestone 1

OpenGL functions: macOS links the system framework; elsewhere a short list of function
pointers is loaded at start-up (`render/gl.hpp`), so no loader library is needed.
- stb_image, stb_image_write: images
- Roboto (embedded)

## 12. Repo layout

```
src/core/      mesh, edit ops, smoothing groups, soft selection and falloff, parts,
               texture coords (projection, unwrap, weld), images, painting, problems,
               document, commands
src/io/        .mazz reader/writer, .glb/.gltf and .obj export, PNG, layout image
src/render/    GL setup, shaders, model views, texture view, reference images
src/app/       editor (frame, actions, menus), tools (select, vertex, primitives, handles,
               pivot), files, scripts; later tools get their own files here
src/ui/        layout, panels, menus, palette, parts, smoothing groups, problems list
src/platform/  file dialogs, tablet input, file watching
src/app/       main loop, settings, startup
cmake/         dependency pins, file embedding, Info.plist
tests/         core + io tests, sample models, UI scripts
docs/          this file, PLAN.md, LOG.md, DEV.md, HANDOFF.md, THIRD_PARTY.md,
               releases/, screenshots/
.github/       macOS Intel builds + tests, Apple Silicon on demand, Linux tests
```

Pieces that Scalpellino already has (file dialogs, settings storage, Roboto/file
embedding, the UI script runner, CI workflows, `Info.plist` template) are copied from
the Scalpellino repo and adapted. The two repos don't depend on each other.

## 13. Testing

- Core and IO as a library with unit tests on every push (macOS Intel, Linux).
- `.mazz` round-trip golden tests: load → save → identical output.
- Mesh op tests: no zero-area faces, no duplicate corners, no faces over four corners,
  consistent winding after every tool; lathe, loft, bevel, bridge, shell, subdivide and
  push/pull keep a closed surface closed; draw-on-face splits stay in the face's plane;
  clone counts and spacing; weld, mirror and flatten tests.
- Smoothing tests: normals per group, auto smooth by angle.
- Soft selection and vertex brush tests: falloff values, distance modes, locks, mirror
  symmetry, hidden parts untouched, undo exact.
- Texture coordinate tests: each projection, unfold/flatten on known shapes, pixel
  snapping, weld/unweld, stitch, even out, coordinates kept by extrude and divide.
- Painting tests: screen point → face → texture pixel; stroke fill-in; undo exact.
- Problems tests: each problem detected and each fix repairs it.
- Generator and random tool tests: same seed gives identical output; no problems over
  many seeds (rock closed and free of self-intersection, triangle count within ±20 % of
  Faces, flat bottom at the cut height; tree branch counts as typed, cards double-sided);
  clone rotate/scale positions exact for each pivot, "scale the offset" follows a
  geometric series; jitter within Amount, locks exact, mirror exact, welded corners
  stay welded; scatter bases on picked faces, aligned to normals, no overlaps when on;
  image card size and proportions exact, trim matches opaque bounds, crossed cards share
  the centre line; generated textures use only the chosen colours and tile cleanly.
- Export tests: `.glb`/`.gltf` read back with cgltf (counts, cm → m, colours, samplers
  per filtering setting,
  alpha, double-sided, parts and their origins); `.obj` parsed back.
- Scripted UI tests with screenshots into `docs/screenshots/`; an idle CPU check with
  everything open (see DEV.md).
- Second display and tablet input tested by hand.

## 14. Roadmap

| # | Milestone | Status |
|---|-----------|--------|
| 0 | Skeleton: GLFW + OpenGL + ImGui docking, idle = zero, Roboto, menus, four views + maximise, 3D orbit/fly, grid keys, counter, shortcut scheme, CI | done (v0.0.0) |
| 1 | Mesh basics: primitives (plane, box, cylinder, cone, tube, torus, sphere, geosphere), rock, selection modes, transform + clone (with rotate/scale per copy), mirror, flatten, model pivot, undo, `.mazz`, reference images (transparency, front/back), scale figure, `.glb`/`.gltf` + `.obj` export (geometry) | |
| 2 | Modelling: draw on faces + polygon tool, push/pull, inference snapping, extrude/inset/bevel, bridge, shell, divide (subdivide plain/smoothed, edges, poke, connect), cut, loop cut, slide, merge/collapse/weld, dissolve, fill, split, flip, diagonals, double-sided, mirror mode | |
| 3 | Lathe + loft, parts with pivots, smoothing groups + auto smooth, problems list with fixes | |
| 4 | Vertex tool soft selection + vertex brush, jitter points | |
| 5 | Texture coordinates: textures, mapping tool (projections, unfold, flatten), texture view (select, transform, weld/unweld, stitch, regroup, tile), texel check + even out, transparency, image card, file watching, layout image export, scaled import; exports with textures | |
| 6 | Pixel painting: tools, palette + saved palettes, painting on texture view and model, paint view, stroke undo, moved-paint warning, generate texture | |
| 7 | Vertex colours: colour brush, fill, split colours; exports with colours, unlit option; scatter on faces | |
| 8 | Paint window + tablets: the second window's full painting UI (texture and vertex colour painting, vertex brush), pen pressure, double-tap, position remembered; hand test with an iPad | |
| later | Tree generator | |
