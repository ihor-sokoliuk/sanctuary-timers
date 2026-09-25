# Sanctuary Timers implementation plan

**Goal:** Deliver a small native Windows overlay implementing the approved Diablo event specification.
**Architecture:** Testable C++ scheduling/model core, WinHTTP adapter and portable state store, event-driven Win32 window with independent background alpha and opaque text. A worker runs bounded network requests; the UI owns state and schedules the next required wakeup.
**Spec:** [design.md](design.md) (copy of the approved specification).
**Execution:** Inline, test first, then non-interactive live verification. The user has already authorized implementation, launch alongside Diablo, startup registration, and public source publication. No further planning checkpoint is needed.

## Constraints
- One compact window: skull/World Boss, flame/Helltide, swords/Legion; no names, locations, maps, sound, notifications or flashing.
- Appearance editing, collapse and drag remain inside the panel. No foreground activation.
- Native runtime only. Render at most once per second while expanded and visible; no polling for game focus.
- WinEvent focus/location tracking, relative placement and DPI handling.
- Local UTC calculation; event-specific sync 20 seconds after boundaries; cache, validation and capped failure backoff.
- Automatic per-user startup, preserving Task Manager disablement and user removal.
- Unit tests pass before any overlay executable is launched. Never synthesize user mouse or keyboard input while testing.

## Tasks
1. [x] Core: write failing tests for UTC parsing, boundary calculations, record validation, changed schedules, stale cache, transition batching, backoff, inactive deferral and clock changes. Implement and pass.
2. [x] Preferences/platform: failing tests for normalization, placement, startup decision policy, persistent file roundtrip and transport schema. Implement, pass, add bounded WinHTTP adapter.
3. [x] Native UI: use tested layout/hit-testing and state decisions; implement no-activate layered panel, icons, contained appearance controls, tray, focus/location notifications, background requests and atomic state persistence.
4. [x] Package/build: repeatable compiler command, CMake support, Windows CI, icon/version, MIT license, README, user-facing build artifact.
5. [x] Verify: full tests, review, isolated integration check, live passive launch, resource measurement, exact limitation report. Publish source/docs/tests and workflow to a new public repository.

## Review focus
- Bad or future timestamps must never poison persisted schedule anchors (task 1).
- Failed requests and repeated foreground transitions must not cause a retry storm (task 1).
- Resizing or monitor removal must not strand controls offscreen (task 2).
- Startup repair must never undo the user's disable/removal choice (task 2).
- Destroy/resume/worker completion must not race shutdown or take game focus (task 3/5).

## Execution record
- 2026-09-25: confirmed Diablo IV is running; no existing native compiler found. Downloading a checksum-verified portable Zig C++ toolchain into scratch, with no system installation.
- Ruling: use a new source directory under outputs rather than a worktree: this task has no Git repository or existing source. Initialize an isolated new repository there.
- Ruling: existing projects are design references only; write the small core and UI independently, avoiding inherited unfinished UI and irrelevant features.
- Core, platform and rendering suites pass before normal launch. Review added regression coverage for changed/exhausted schedule anchors, backward clock changes, request cancellation/deadlines and text widths. Forced Direct2D resource recreation is covered by an offscreen renderer check.
- A 973,312-byte baseline x64 executable passed an offscreen no-activation smoke check. Initial normal launch preserved the foreground application and registered per-user startup.
- Live observation verified all three feeds, cache writes, focus/DPI changes, single instance, native window styles, and appearance/position changes during the user's own interaction. At 21:55:20 UTC the Helltide boundary triggered exactly one Helltide read; Boss and Legion counts remained unchanged. No input was synthesized. See validation.md for measurements and manual acceptance limits.
- Published https://github.com/ihor-sokoliuk/sanctuary-timers. Independent Windows MSVC CI passed the build and all three test suites. Delivered a portable ZIP with runtime notices; private preferences, logs and cache are excluded.
