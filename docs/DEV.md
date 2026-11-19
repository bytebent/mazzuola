# Mazzuola — Dev tools, build and testing

Everything needed to build, test and release. Rules are in [`../CLAUDE.md`](../CLAUDE.md),
design in [`ARCHITECTURE.md`](ARCHITECTURE.md), status in [`PLAN.md`](PLAN.md), history in
[`LOG.md`](LOG.md).

## Build

- **Mac:** `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j`
  (CMake 3.24+, Apple command line tools). The app is `build/Mazzuola.app`.
- **Linux** (CI and the AI workspace): needs `libxrandr-dev libxinerama-dev libxcursor-dev
  libxi-dev libgl1-mesa-dev libgl1-mesa-dri libxkbcommon-dev xvfb` (`apt-get update`
  first). The binary is `build/Mazzuola`.
- **Dependencies** (GLFW, Dear ImGui, stb) are fetched by CMake at pinned versions, see
  `cmake/Deps.cmake` and the table in `PLAN.md`. To update one, change the URL and note
  it in the LOG.
- **AI workspace:** GitHub archive downloads are blocked, so FetchContent fails. Clone the
  deps with git at the pinned refs into a scratch folder and configure with
  `-DFETCHCONTENT_SOURCE_DIR_GLFW=… -DFETCHCONTENT_SOURCE_DIR_IMGUI=…
  -DFETCHCONTENT_SOURCE_DIR_STB=…`.
- **OpenGL functions:** macOS links the system framework; elsewhere `render/gl.hpp`
  loads a short list of function pointers. A new GL function must be added to that list
  (and its `#define`).
- **macOS specifics:** OBJC and OBJCXX are both enabled at the top of `CMakeLists.txt`
  (without OBJC, GLFW's Cocoa `.m` files get compiled as Objective-C++ and fail).
  `GLFW_INCLUDE_NONE` keeps GLFW from pulling in the legacy OpenGL header.

## Core tests

- `ctest --test-dir build --output-on-failure`, or run `./build/mazz_core_tests` directly
  (`tests/test_core.cpp`; it prints the number of checks and failures).
- **Probes:** to reproduce a bug in numbers, write a small `.cpp` in a `mktemp -d` folder
  and link it against `build/libmazz_core.a`. Fix, then keep a permanent test.

## Scripted UI tests

- Scripts are `tests/ui/*.script`, run on a virtual screen (Linux + Xvfb) by
  `tests/ui/run.sh <absolute path to build/Mazzuola> [screenshot-dir]`. Each script runs
  in a fresh temp folder. A failed `expect` makes the script, and the run, fail.
- Run them at 1× and at Retina-like scale: `MAZZ_VIEW_SCALE=2 tests/ui/run.sh …`.
  CI does both. GitHub's Mac VMs have no OpenGL, so UI tests only run on Linux.
- **Duplicate IDs:** on every scripted frame a check (`src/ui/imgui_checks.cpp`, using
  Dear ImGui's item hooks, `IMGUI_ENABLE_TEST_ENGINE`) fails the run if two widgets share
  an ID. ImGui itself only notices while the mouse hovers them.
- One command per frame (`wait N` skips frames). Mouse input goes through ImGui's input
  queue, keys through the same path as real key presses. End a test script with `quit`;
  without it the app stays open and goes idle (that's how the idle check works).
- **Hover lag:** which view is hovered is known one frame after a mouse move, so put
  `wait 2` after `vmove` before a `key` that depends on it (e.g. `Z` for fly mode).
- Errors print as `FAIL script: line N: …` on stderr.

### Script language

```
wait <frames>                      do nothing for a number of frames
key <chord>                        e.g. key Shift+R, key è, key +, key Cmd+0, key F1
action <id>                        run an action as if clicked in a menu (ids in src/ui/actions.cpp)
vmove <view> <fx> <fy>             move the mouse to a spot in a view image (0..1 fractions)
press <left|right|middle>          press a mouse button where the mouse is
release <left|right|middle>
wheel <amount>                     scroll the mouse wheel (positive = up)
spacedown / spaceup                hold / let go of Space (Space + left drag pans)
uikey <Escape|Enter>               a key press for the UI only (dialogs), not a shortcut
type <text>                        type text into the focused field
viewdir <view> <Top|Bottom|Front|Back|Left|Right|3D>
maximise <view> / restore
grid <cm>                          set the grid size
camera <view> <yaw> <pitch> <distance> <tx> <ty> <tz>   place a 3D view's camera
flymove <forward> <right> <up>     move the flying camera (cm)
shot <file.png>                    screenshot of the main window after this frame
paintshot <file.png>               screenshot of the paint window after its next frame
quit                               end the run
expect grid <cm> | gridvisible <0|1> | snap <0|1> | mode <vertices|edges|faces|pieces>
expect viewdir <view> <name> | maximised <view or -1> | flying <0|1>
expect cam <view> <yaw> <pitch> <distance>      (±0.5)
expect ortho <view> <cu> <cv> <zoom>            (±1 %)
expect controls <0|1> | paintwindow <0|1>
expect dirty <0|1> | undo <steps> | faces <n> | verts <n> | selected <n>
expect title <text>                 window title
expect tool <Select|Primitives>     the active tool
expect fileline <file> <line>       the file contains exactly that line
saveas <file> | open <file>         save / open without dialogs (relative to the temp folder)
addbox <x0 y0 z0 x1 y1 z1>          test helper: add a box as one undo step, selected
vworld <view> <x> <y> <z>           mouse over a world point as that view shows it
hold <shift|cmd|alt> / letgo <...>  hold / release a modifier for the next mouse actions
expect bounds <x0 y0 z0 x1 y1 z1>   whole model bounds;  selbounds: the selection's
expect preview <0|1>                the Primitives tool shows a preview box
expect previewfaces <n>             faces of the previewed shape
prim <shape> / primset <setting> <value>   choose a shape / set a setting (as in Properties):
                                    segx segy segz rows cols sides segments caprings hole
                                    tubesides tubewidth stacks iterations
                                    rockfaces roughness flatbottom bottom quads seed
```

Views are numbered 0–3: top-left, top-right, bottom-left, bottom-right.

## Command line

- `--script <file>`: run a script. `--screenshot <png>`: render a few frames, save a PNG
  in the current folder, exit.
- Automated runs never read or write the user's layout settings.
- Linux headless: `xvfb-run -a -s "-screen 0 1600x1000x24" build/Mazzuola --screenshot x.png`.
- Environment: `MAZZ_VIEW_SCALE=<n>` overrides the scale of the view images (tests only).

## Idle check

The app must cost ~zero CPU when nothing happens (`src/app/main.cpp` describes the idle
policy), with everything it can show open, not only an empty app.

- `tests/ui/idle.sh <absolute path to Mazzuola> [seconds]` (Linux, needs Xvfb): starts the
  app on its own virtual screen with `tests/ui/idle/scene.script`, which changes the
  views, opens the Controls window and the paint window, and has no `quit`, so the app
  goes idle. The screenshot at the end is the "ready" signal; the harness waits 3 s more,
  then reads the process's CPU time (user + system, all threads, from `/proc`) twice,
  10 s apart.
- **Pass = 0 ticks** (1 tick = 10 ms). `IDLE_MAX_TICKS=<n>` raises the allowance if a CI
  runner proves noisy. Run it at 2× too: `MAZZ_VIEW_SCALE=2 tests/ui/idle.sh …`.
- **The scene is checked:** the script `expect`s its contents, and the harness fails if
  the app printed any error while building it.
- **Control:** `tests/ui/idle.sh <app> --control` runs a script that keeps the app busy
  and must see at least 10 ticks, proving the measurement can see CPU use.
- CI runs all three (control, 1×, 2×) in the `linux` job.
- When something new can be shown (a model, textures, the texture view in use...), add
  it to `scene.script`: a new feature that makes idle cost CPU then fails this test.

## CI

`.github/workflows/build.yml`:
- **macos:** Intel (`macos-15-intel`): build, core tests, ad-hoc sign, zip
  `Mazzuola-intel.zip`.
- **linux:** build, core tests, UI scripts at 1× and 2×, the idle check (control, 1×, 2×),
  screenshots as an artifact.
- **release:** after both pass, on pushes to `main`: replaces the "Latest build"
  pre-release, and makes a version release for every `docs/releases/vX.Y.Z.md` that has no
  release yet.
- Apple Silicon builds are on demand only: Actions tab → **apple-silicon** → Run workflow.
- The macOS Intel job sometimes waits in the queue and gets cancelled (no steps run).
  Re-run it: `gh api -X POST repos/bytebent/mazzuola/actions/runs/<id>/rerun-failed-jobs`.

## Releasing

Version: `v0.<milestone>.<patch>`, set in `project(... VERSION ...)` in `CMakeLists.txt`.
Add `docs/releases/vX.Y.Z.md` (append `docs/releases/INSTALL.md`) and push to `main`. CI
makes the tag and release after the builds pass (about 7–8 minutes). Tag pushes are
blocked in the AI workspace, so this file-driven route is the one to use.

## Per-step checklist

1. Build, then core tests, then all UI scripts at 1× and 2×.
2. Idle check (above): `tests/ui/idle.sh`, control, 1× and 2×.
3. Screenshots into `docs/screenshots/` (`m<milestone>-<nn>-<what>.png`), mentioned in the
   LOG entry.
4. `docs/PLAN.md` status, a `docs/LOG.md` entry, release notes if releasing, README and
   Help → Controls kept in sync.
5. Commit as `Claude <noreply@anthropic.com>`, e.g.
   `git -c user.name=Claude -c user.email=noreply@anthropic.com commit …`. Push, then check
   CI (`gh api repos/bytebent/mazzuola/actions/runs`).

## Workspace quirks (AI cloud workspace)

- `gh` GraphQL commands (e.g. `gh release list`) are refused. Use REST:
  `gh api repos/bytebent/mazzuola/releases`, `gh api repos/bytebent/mazzuola/actions/runs`.
- Creating, editing or deleting releases through the API is refused here (HTTP 403):
  ask the user to do it on GitHub.
- Use `mktemp -d` for scratch work.
- There is no Mac or iPad here: anything macOS-only (pen input, Sidecar, Retina look) is
  compiled by CI and checked by the user by hand.
- Measure before fixing: a probe reproduces the bug in numbers, the fix makes it zero, a
  permanent test keeps it zero.
