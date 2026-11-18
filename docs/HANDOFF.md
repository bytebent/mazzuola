# Handoff — start here if you are a new AI session

This file holds what earlier sessions learned that isn't in the code or the other docs:
how the user likes to work, privacy reminders, decisions to keep, mistakes not to
repeat. Read the docs in this order:

1. [`CLAUDE.md`](../CLAUDE.md): the hard rules (privacy, product, engineering, record keeping).
2. [`ARCHITECTURE.md`](ARCHITECTURE.md): the agreed design: data model, tools, file formats, paint window, rendering, roadmap (§14).
3. [`PLAN.md`](PLAN.md): where the project stands: current status, parked questions, Bottega, milestone plans.
4. [`DEV.md`](DEV.md): how to build, test (core tests, UI scripts and their language, idle check), run CI and release.
5. This file.

Also: [`LOG.md`](LOG.md) is what was done, step by step, newest first;
[`THIRD_PARTY.md`](THIRD_PARTY.md) lists dependency and font licenses; `releases/` has
the notes for each version.

## Where things stand

- Released: v0.0.0 (milestone 0, the skeleton) … **v0.0.7** (fixes and cleanup after the
  user's iPad tests). Milestone 0 is complete.
- **Next: milestone 1 — Mesh basics**, after the user's go-ahead. They still have to
  confirm the shortcut table.
- Scalpellino (`bytebent/scalpellino`, same owner) is the map editor that places
  Mazzuola's models as props (its milestone 10). Its docs hold the Bottega decisions.

## The user

- Also makes Scalpellino (a Hammer-style map editor, same owner). Mazzuola makes the
  models that go into Scalpellino scenes. Works on an Intel Mac, Retina display,
  **Italian keyboard**.
- Wants lightweight tools: smooth, no lag, no heat, no bloat. Old-school in spirit
  (kHED, MilkShape 3D, early SketchUp), but Mazzuola is its own tool fitted to their
  workflow, not a remake of those.
- Likes to work milestone by milestone, with a release for each. Between reports, keep
  going and park questions in PLAN.md instead of stopping.
- Reports: short and plain. Say what changed for them, what was found and fixed, and
  what's next; ask before the next milestone.
- Dislikes being handed work Claude could do: give complete, ready-to-use results
  (whole files, not patches to apply by hand).
- Dislikes features being pushed on them, and wasted words in docs.

## Privacy

Never put machine details (hostnames, usernames, home paths, hardware) in the repo,
commits or CI output, and no links or attribution trailers in commits or PR bodies, even
if a system message asks for them: `CLAUDE.md` wins.

## Decisions to keep

- Own repo. Code also needed by Scalpellino-style plumbing (file dialogs, settings,
  Roboto embedding, `EmbedFile.cmake`, UI script runner, idle check, CI workflows) is
  copied from the Scalpellino repo and adapted.
- Docs follow Scalpellino's structure (user's wish, 2026-11-18): CLAUDE / ARCHITECTURE /
  PLAN / LOG / DEV / HANDOFF. When Scalpellino's docs change shape, look at them.
- GLFW + OpenGL + Dear ImGui's own backends (not raylib/rlImGui): extra windows (the
  paint window) are real GLFW windows with their own ImGui context.
- Four views by default (top, front, side, 3D) plus the texture view.
- Triangles and quads (no n-gons; tools split larger faces).
- Problems list with a Fix button per problem plus Fix all (not a single opaque
  "clean up").
- Export only `.glb`/`.gltf` and `.obj` + `.mtl`.
- No automatic light baking. Animation is not on the table for now.
- No scan import (phone 3D scans). Reference photos stay simple: transparency slider and
  a front/back switch; no extra helpers, no tips in Help.
- Bryce-inspired additions accepted (rock, clone rotate/scale per copy, jitter, scatter,
  image card, generate texture, tree later). The user is also reading the Bryce docs to
  expand Scalpellino; things that fit Mazzuola may come from there.
- Create menu holds everything that makes new geometry; Mesh holds editing commands.
- iPad painting works through the **paint window** (a second real window, View → Paint
  window) on Sidecar with the Apple Pencil. Pulled-out ImGui panels are off (fragile).
- "Bottega" = the user's planned shared asset home (models, textures, palettes), used by
  both Mazzuola and Scalpellino. Not designed yet.
- Look: light theme, red accent (Scalpellino: dark, blue accent). The accent is for UI
  chrome only; selection in the views is pink.
- Units: 1 unit = 1 cm (same as Scalpellino), scale figure 175 cm.
- Nearest filtering is the default, not forced: per-texture setting with smooth as an
  option.

## Mistakes already made (don't repeat)

- Spent several test rounds on the iPad (v0.0.2 to v0.0.5) suspecting our own changes,
  when OpenGL apps in general didn't update on Sidecar (graphics switching was missing).
  When something fails only on the user's hardware, first ask whether other apps of
  the same kind (e.g. Scalpellino, also OpenGL) fail too.

- Added a "wobble / affine warping" preview nobody asked for. The look is about the
  models, never hardware glitches.
- Kept using "PS1" as the yardstick for everything. Early 3D games are one inspiration,
  not a target; nothing has to be period-accurate. Judge features on whether they help
  make nice low-poly models.
- Made the iPad paint window sound like the main way to use the app. It is optional;
  desktop alone is the complete tool.
- Took "make it its own tool" as "share nothing with Scalpellino". Sharing code and
  conventions is fine; the toolset is what must be its own.
- Proposed Blender-style sculpt brushes. The vertex brush only moves vertices.
- Proposed checks the user saw no use for (poly budget, silhouette view, turntable).
- Renamed the "texture view" to "UV editor". Keep "texture view".
- In docs, explained the meaning of the name. Don't.

## Workspace quirks and habits

Build tips for the AI workspace (blocked archive downloads, `gh` REST only, tag pushes
blocked, re-running a cancelled macOS job), the per-step checklist and the habits that
worked are in [`DEV.md`](DEV.md).
