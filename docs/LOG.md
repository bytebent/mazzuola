# Mazzuola — Log

What was done, step by step, newest first. Recent work lives here; when this file gets
long, move the oldest entries to `LOG-archive.md` (newest first there too). Status, plan
and open questions are in [`PLAN.md`](PLAN.md).

### 2026-11-19 — Redesign: selecting, tool bar (docs only)
- Agreed: selecting click by click (a click picks a whole shape by default; 1–3 finer,
  4 back; click again to reach what's underneath in 2D; Cmd+click; rectangle on empty
  space; empty click / Esc selects nothing; selection box in every view).
- Agreed: the main tools as buttons on the left, as in Scalpellino; no panel listing
  every tool. ARCHITECTURE §6 layout and Selection updated.

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
