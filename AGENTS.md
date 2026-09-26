# Working on Sanctuary Timers

Sanctuary Timers is a lightweight native Windows overlay for Diablo IV World Boss,
Helltide and Legion countdowns. These instructions apply to this repository.

## Start here

- Inspect `git status --short` and preserve unrelated changes. Read [README.md](README.md)
  for current behavior, [docs/design.md](docs/design.md) for product decisions, and
  [docs/production-deployment.md](docs/production-deployment.md) for deployment history.
- Use current source, process state and release metadata to verify facts that may
  have changed. Recorded PIDs, versions, hashes and test counts are historical evidence.
- Investigation and review requests are read-only. Implement explicitly requested
  changes without repeated permission questions. The user's standing instruction
  for this project is to publish and install/deploy changes automatically: complete
  relevant tests, push, publish a versioned release for application changes, and
  update the managed local installation without another approval question. This
  includes the necessary overlay restart. Documentation-only changes need a push,
  not an application release/restart. A later explicit restriction takes precedence;
  this authorization does not extend to unrelated applications or messaging.
- Invoke Claude only when explicitly requested for a named artifact. Do not impose
  a particular model, delegation workflow or extra approval gate on routine work.
- The user may be gaming. Do not synthesize mouse/keyboard input, activate windows,
  restart Diablo, sign out, reboot, or close Codex as a verification shortcut.
  Prefer passive inspection and offscreen tests. Launch helpers hidden.

## Find the implementation

| Files | Responsibility |
| --- | --- |
| `src/core.h`, `src/core.cpp` | Event parsing, recurrence, scheduling/backoff, preferences normalization, layout and hit testing |
| `src/clock.h`, `src/clock.cpp` | Pure SNTP packet validation, corrected clock and daily synchronization policy |
| `src/clock_network.cpp` | Windows Sockets transport, bounded asynchronous DNS, time-server query and cancellation |
| `src/platform.h`, `src/platform.cpp` | UTC fallback, INI persistence, WinHTTP event adapter, startup registry and game-window helpers |
| `src/tracking.h`, `src/tracking.cpp` | Window readiness, discovery-event filtering, bounded focus recovery and diagnostic classifications |
| `src/main.cpp` | Win32 message loop, foreground/location hooks, workers, timers, tray and diagnostics |
| `src/render.h`, `src/render.cpp` | Direct2D/DirectWrite drawing, private embedded fonts, text metrics and bitmap export |
| `resources/` | Executable version/icon and embedded PT Serif fonts |
| `tests/` | Core, clock, platform, rendering and PowerShell installer regression tests |
| `scripts/` | Managed per-user installer and versioned release packaging |
| `.github/workflows/windows.yml` | Windows MSVC build, tests and downloadable release artifacts |

The application uses C++20 and Windows APIs. Keep the native executable independent
of browser engines and separately installed language runtimes. The installer targets
Windows PowerShell 5.1; test compatibility there, not only in PowerShell 7.

## Preserve the product behavior

- Keep one compact panel with event icon, event name and countdown. All text uses
  embedded PT Serif Bold. Labels match countdown colors: boss red, Helltide yellow-orange,
  Legion blue-gray. Preserve these colors for cached/offline data too; use `~` to mark
  estimates, never a shared gray override.
- Width changes in 5-pixel steps. Its minimum depends on measured bold text width;
  preserve the 8-pixel label/countdown gap and test all supported font sizes (11–22).
  At 15-pixel text the current minimum is 241 logical pixels, not a universal constant.
- Follow Diablo's window/monitor with WinEvent notifications. Keep automatic
  windows non-activating and topmost, the drawing surface click-through, and controls
  contained inside the panel. Hide when another app is foreground except for the
  user's explicit tray Appearance action. Suspend hidden redraws. Show/restore events
  supplement foreground events; transient detection gets at most six retries within
  four seconds, with no continuous focus polling or background-window activation.
- Do not add game-memory reads, injection, gameplay automation, telemetry or alerts
  as incidental implementation choices. Preserve preferences and cached event data.

## Keep event refresh and clock synchronization separate

- Event countdowns are calculated from absolute UTC and validated feed anchors.
  Helltide uses a 55-minute active period plus 5-minute break; current boss/Legion
  recurrence is 210/25 minutes. These are source-validated rules, not promises from a
  Blizzard API. Recheck the adapter when real feed behavior changes.
- Fetch the three event records on a new active game session. Refresh only the
  affected record 20 seconds after its known boundary. Defer event reads while the
  game is not foreground; coalesce missed transitions. Never add a five-minute poll.
  Failed event reads retain cache, back off from 1 to 30 minutes, and honor `Retry-After`.
- The app's clock checks `time.windows.com` at launch and every 24 hours, even while
  Diablo is closed. Use the corrected monotonic time for countdowns and scheduling;
  align display ticks to corrected second boundaries. Catch up once after overdue sleep.
- Clock failures retain the last in-process correction, or Windows UTC before the
  first success. Retry after 15 minutes with exponential backoff capped at 6 hours.
  Do not persist/reapply an old offset across restarts or modify the Windows system clock.
- Settings distinguish **Events checked** (the oldest successful event-record read)
  from **Clock: synced ... (daily)**. A long event-check age is not proof of clock drift.
- Keep network operations off the UI thread and bounded/cancellable. Preserve reply
  validation, duplicate suppression, cache fallback and ownership of posted results.

## Build and verify before running changes

Run from the repository root. Prefer the existing compiler; do not install another
toolchain unnecessarily. With Zig 0.15.2 available on `PATH`:

```powershell
.\build.ps1 -Zig zig
```

In the original Codex workspace, its verified local-toolchain location is
`..\..\work\toolchain\zig-x86_64-windows-0.15.2\zig.exe`; pass that path to `-Zig`
if it still exists. `-TestsOnly` omits the final application build. The script runs
core, clock, tracking, platform and offscreen rendering tests before producing
`build\SanctuaryTimers.exe`; it does not launch the overlay or register startup.

MSVC alternative:

```powershell
cmake -S . -B build-msvc -A x64
cmake --build build-msvc --config Release --parallel 2
ctest --test-dir build-msvc -C Release --output-on-failure
```

Installer changes also require Windows PowerShell 5.1:

```powershell
& "$env:WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File .\tests\installer_tests.ps1
```

- Add meaningful failing regression tests for timing/behavior fixes before implementation.
  Use injected times for daily boundaries, sleep and clock jumps; never advance the
  Windows clock or shorten the production daily interval to test it.
- Rendering tests check actual embedded fonts and glyph widths, resource recovery,
  alpha and foreground preservation. `build\render_tests.exe <output.bmp>` also
  exports the offscreen settings preview. Inspect it for layout changes.
- `build\platform_tests.exe --live-clock` performs one real SNTP query. It is an
  explicit integration check, not part of ordinary offline tests; do not poll it.
- Normal development launches can register their executable path for startup. Prefer
  tests. `--smoke` must run without another overlay instance, and requires fresh
  `smoke-result.txt` evidence; a zero exit caused by the singleton guard proves nothing.
- Keep build load modest during gameplay. Use limited parallelism/BelowNormal
  priority when useful. Documentation-only edits need link/diff checks, not app restarts.

## Publish and deploy changes

1. Update both `CMakeLists.txt` and `resources/app.rc` for a new binary release.
   Keep numeric and string resource versions consistent. Pass relevant local tests.
2. Commit/push within the authorized scope and require successful Windows CI for the
   exact source commit. Prefer that run's `SanctuaryTimers-win-x64` artifact for release.
3. Packaging is defined by `scripts/New-ReleasePackage.ps1 -Executable <built.exe>`.
   Release assets are `SanctuaryTimers-X.Y.Z-win-x64.zip`,
   `Install-SanctuaryTimers.ps1` and `SHA256SUMS.txt`; tags are `vX.Y.Z`.
   Verify hashes/content and publish a new stable version rather than replacing old assets.
   Use a notes file for multiline `gh` release descriptions.
4. Download and verify the published installer, then run it with Windows PowerShell 5.1
   and `-Version X.Y.Z`. For first migration only, use `-MigrateFrom <portable-folder>`.
   Do not deploy by merely leaving a Codex-owned development process running.
5. Preserve preferences, event cache, unrelated startup values and Task Manager's
   enabled/disabled choice. Verify the installed EXE version/hash against the release,
   one responsive process, correct startup path, independent Windows parent and
   survival after the installer exits. Check actual clock/event health, not only task status.
6. Record relevant evidence and limitations in `docs/validation.md` or
   `docs/production-deployment.md`. Distinguish unit/CI success from live verification;
   do not claim reboot, sign-in, game visibility or a full-day cycle without observing it.

## Understand the installed copy

- Default directory: `%LOCALAPPDATA%\Programs\SanctuaryTimers`. Runtime files live
  beside the executable: `settings.ini`, `events.ini`, `diagnostics.log`, `status.txt`,
  and installer ownership/version metadata `install.json`.
- Windows sign-in startup uses the quoted EXE path in the current user's Run value
  **Sanctuary Timers**. Task Manager remains authoritative; never change its
  `StartupApproved` metadata to override disablement.
- The **Sanctuary Timers - <current-user SID>** scheduled task is an on-demand
  independent launcher with an interactive user token, no automatic triggers, no
  runtime limit and battery operation allowed. Do not add a second automatic startup
  path that bypasses Task Manager.
- The installer registers Start menu/Installed apps entries, safely quits the owned
  instance, and keeps `SanctuaryTimers.previous.exe` for update recovery. Do not delete
  that backup before recovery is confirmed. Uninstall retains preferences/cache and
  an ownership marker so later reinstall works.
- Never use `New-Item -Force` to initialize an existing Run registry key: it can
  erase unrelated values. Use the installer's conditional initialization helper.

## Inspect a running instance without taking control

`--status` asks the running app to write `status.txt`; `--snapshot` saves its own
rendered `preview.bmp`; `--quit` requests a graceful exit. Require fresh output and
the correct executable/process identity when interpreting diagnostics.

Windows PowerShell 5.1 `Start-Process -ArgumentList` can append a trailing space to
these raw commands, which the native parser rejects. Reuse the tested helper:

```powershell
. .\scripts\Install-SanctuaryTimers.ps1
$installedExe = Join-Path $env:LOCALAPPDATA 'Programs\SanctuaryTimers\SanctuaryTimers.exe'
Invoke-OverlayCommand $installedExe '--status'
```

Dot-sourcing loads functions without installing anything. Wait for a newly written
status file before reading it. Useful fields include `startup_ok`, `paint_errors`,
`event*_reads/failures/checked`, `clock_synced`, `clock_reads`, `clock_failures`,
`clock_offset_ms` and `clock_next_ms`. Do not force focus merely to make `visible=1`.
For launch detection, also check `window_state`, `window_error`, `discovery_hooks_ok`,
`focus_retry_checks` and `focus_retry_pending`; logs identify the event and rejected state.

Keep machine-specific logs, cached state, toolchains and temporary evidence out of
Git/release assets. The packager copies `docs/`, so put only sanitized documentation
there. Historical workspace scripts may contain old expected versions; inspect them
before reuse. Preserve user data and validate resolved paths before recursive cleanup.
