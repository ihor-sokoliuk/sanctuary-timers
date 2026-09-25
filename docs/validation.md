# Validation record

Local verification on Windows x64, 2026-09-25, using Zig 0.15.2 targeting baseline `x86_64-windows-gnu`:

- 28 core cases passed: parsing/validation, event boundaries and local recurrence, malformed data, changed schedules, cache estimates, deferred/coalesced refreshes, retry behavior, clock jumps, request budget, settings, placement, startup policy and hit testing.
- Platform suite passed: preferences/cache file roundtrips, corrupted input fallback and executable/window helpers. This suite uses temporary files and does not register startup or use the network.
- Rendering suite passed: actual DirectWrite text metrics for all 12 supported font sizes, countdown/label fit, independent background alpha and opaque glyphs, forced device-resource recreation, hidden drawing and foreground preservation.
- Offscreen executable smoke check passed: successful layered render, correct no-activate/click-through styles, no visible window and no foreground change.
- Packaged executable is 973,312 bytes and uses the Windows GUI subsystem. It requires no separately installed language runtime.
- First normal launch registered the quoted executable path in the current user's Run key. With another application foreground, it made zero event reads and zero paints. Passive live measurement is ongoing.

Core behavior and review regressions were exercised as failing tests before their fixes. The rendering integration suite was then used to verify native drawing and resource recovery. All three suites passed before normal application launch.

## Limits

No mouse or keyboard input is synthesized during verification. User-driven dragging, collapse, settings, Alt+Tab and monitor moves require separate manual confirmation if not naturally exercised during play. Pure startup policy tests cover preserving disablement/removal; Windows logon and actual Task Manager toggling have not been exercised. Request budget/cancellation logic is tested, but a hostile slow-chunk HTTP server is not part of the suite. A cancelled request may finish its current WinHTTP operation (up to the configured 4-second timeout) before exiting. No comparative performance claim against another overlay has been measured.
