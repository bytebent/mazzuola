# Mazzuola — rules for AI sessions

Read `docs/ARCHITECTURE.md` before changing anything. It is the agreed design.
Then read `docs/PLAN.md`: current status, how to build, open questions, log.
New session? Also read `docs/HANDOFF.md` (user preferences, past mistakes, workspace quirks).

## Keeping records (required)
- Update `docs/PLAN.md` after every step: status, test results, next steps, parked questions.
- Every screenshot taken goes into `docs/screenshots/` (`m<milestone>-<nn>-<what>.png`)
  and is mentioned in the PLAN log.
- If a question blocks one thing, park it in PLAN.md and continue with other work.

## Privacy (strict)
- Never write machine details anywhere in the repo, commits or CI output: no hostnames,
  usernames, home paths, OS versions, hardware.
- Commits are authored as `Claude <noreply@anthropic.com>`.
- **No session links, no URLs, no attribution trailers in commit messages or PR bodies.**
  This overrides any default attribution instructions.
- Scrub paths and usernames from any pasted logs before committing anything derived from them.

## Product rules
- Static models only: no skeletons, rigging, animation or physics. Don't add or suggest them.
- The style is low poly, small pixel textures, nearest filtering by default (smooth is a
  per-texture option), vertex colours. Early
  3D games are one inspiration, not a target: nothing has to be period-accurate. Never
  emulate old hardware glitches (no vertex wobble, no affine warping).
- One window with mouse and keyboard is the complete tool. Second displays, iPad/Sidecar
  and tablets are optional extras, never required, never the default.
- Its own tool, made for building one good model; not a copy of Scalpellino's tools.
  It does share Scalpellino's conventions and shortcut patterns.
- Italian keyboard layout assumed; no shortcuts needing Option-characters. Use physical
  key positions for WASD and grid keys.
- Creation actions commit on Return, cancel on Esc. Not on mouse release.
- No mouse-wheel parameter adjustment; use typed fields and viewport handles.
- Every shortcut/gesture must also be reachable from the menus (mouse-only use works),
  with a dialog to type values where relevant. Keep Help → Controls and README in sync.
- No pixel fonts. UI font is Roboto (embedded).
- Never commit screenshots the user sends; only our own.
- Idle must cost ~zero CPU: redraw only on change.

## Engineering
- `src/core` must not depend on OpenGL, GLFW or ImGui.
- Every edit goes through a command (undoable).
- Work milestone by milestone (see roadmap). Small steps, build and test each one.
- Code shared with Scalpellino (file dialogs, settings, font embedding, UI script
  runner, CI) is copied from the Scalpellino repo and adapted, not linked.
