# Handoff — start here if you are a new AI session

This file holds what earlier sessions learned that isn't in the code, `docs/PLAN.md` or
`CLAUDE.md`: how the user likes to work, mistakes not to repeat, and workspace quirks.
Read in this order:

1. `CLAUDE.md`: hard rules (privacy, product, engineering, records).
2. `docs/ARCHITECTURE.md`: the agreed design and roadmap (§14).
3. `docs/PLAN.md`: current status, parked questions, the log (newest first).
4. This file.

## Where things stand

- Design agreed and written down. Nothing built yet.
- **Next: milestone 0 — Skeleton**, after the user's go-ahead.

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

## Mistakes already made (don't repeat)

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

## Decisions to keep

- Own repo. Code also needed by Scalpellino-style plumbing (file dialogs, settings,
  Roboto embedding, `EmbedFile.cmake`, UI script runner, CI workflows) is copied from
  the Scalpellino repo and adapted.
- GLFW + OpenGL + Dear ImGui's own backends (not raylib/rlImGui), because ImGui
  multi-viewport is needed for the optional detached paint view.
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

## Workspace quirks (AI cloud workspace)

- See Scalpellino's `docs/HANDOFF.md` for the cloud workspace notes (blocked GitHub
  archive downloads → clone deps at pinned refs and pass `FETCHCONTENT_SOURCE_DIR_*`;
  Linux packages for builds; `mktemp -d` for scratch work). Add Mazzuola-specific notes
  here as they come up.
