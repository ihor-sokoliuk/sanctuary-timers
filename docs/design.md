# Diablo event overlay: design

Updated September 25, 2026. This is the approved design implemented by the native application. See [validation](validation.md) for measured results and the interaction checks that remain for the user.

## See three timers while playing

The selected design is B, Edge tab, with the icons from A. A single compact window contains the control rail and three rows:

- Skull icon, **World Boss**, time until its next scheduled spawn.
- Flame icon, **Helltide**, time remaining while active, or time until the next start during the break.
- Crossed-swords icon, **Legion**, time until its next scheduled start.

Boss names and locations are omitted. The product has no maps, outbound event links, sounds, alerts, notifications, or flashing. The control rail contains collapse/expand, a drag handle, and appearance settings. All controls stay within the window. Opening appearance settings expands the same window and replaces the timer rows until closed.

The initial target is approximately 270 by 82 logical pixels at 13-pixel text. Actual native dimensions are subject to font metrics and Windows display scaling. Text size, width, and background opacity are adjustable. Larger text expands the minimum width so labels and times remain readable. Long countdowns use compact hours/minutes/seconds. Text remains legible independently of background transparency.

The September 25 typography refinement uses embedded PT Serif regular for labels and bold for countdowns, with a subtle dark shadow, following the user's in-game screenshot. Fonts load privately from executable resources; no Windows font installation is required.

## Follow the game

Diablo is configured for Windowed Fullscreen. The overlay identifies the game's foreground window and derives placement from that window, rather than a hardcoded monitor. Store the user's placement relative to the game window and clamp it to the available game area after moves, resizes, DPI changes, or monitor removal.

Use out-of-context Windows foreground and window-location event notifications. Resolve the foreground process only when an event requires it. Do not repeatedly enumerate processes on a timer. Perform one initial foreground check at startup. Hide the overlay when another application becomes active. Overlay controls should not activate the window during ordinary interaction; appearance editing may keep the owned settings UI visible, while switching to an unrelated app hides it.

Render countdowns at most once per second while visible. Suspend redraws while hidden. A collapsed panel keeps only its compact control tab. Ordinary display text should not steal keyboard focus or gameplay clicks; rail controls remain interactive, with input behavior verified in the actual game.

## Calculate locally from verified timing anchors

All scheduling uses UTC timestamps. Countdown values are recomputed from absolute time; they are not a counter decremented indefinitely. A monotonic timer drives repaint/wakeups, and wall-clock or sleep/resume changes trigger recalculation.

| Event | Documented rule | Live Helltides evidence collected September 25 |
|---|---|---|
| Helltide | Starts at the top of each UTC hour; active for 55 minutes, followed by a five-minute break | Start 20:00 UTC; end 20:55 UTC |
| World Boss | 210 minutes between scheduled spawns | Start 21:00 UTC; next 00:30 UTC on September 26 |
| Legion | 25 minutes between scheduled starts | Start 20:55 UTC; next 21:20 UTC |

Helltide phase can be calculated from UTC time. Boss and Legion recurrence require a verified timestamp to establish their offset. Do not assume a fixed local time of day: the recurrence advances across days and local DST changes.

For a recurring event with verified start `anchor`, interval `period`, and current time `now`, the next start is the first `anchor + n * period` strictly after `now`. Prefer an explicit future `nextTime` returned by the source over an extrapolated occurrence. Validate the source interval before extending it. If a future source value differs from the expected recurrence, accept the valid source value and mark the extrapolation rule unverified until it can be established again.

At a boss or Legion spawn, display `Now` for 60 seconds, then the next start countdown. This is a scheduled start indication, not a statement that the encounter is still alive. Neither feed provides the time the user's instance finishes the fight, so no fabricated fight-end countdown is used. Helltide start/end transitions are known and switch immediately between `ends` and `in`.

## Synchronize after transitions

There is no repeating five-minute synchronization timer.

1. When Diablo is first detected active in a new game session, load the cache immediately and request the three small event records once.
2. Calculate and display timers locally without network requests while awaiting a known transition.
3. At a Helltide start or end, or a boss/Legion start, update the display immediately from the known schedule. Queue a refresh of only that event's record 20 seconds after the transition.
4. Use one coordinator to combine pending work and prevent duplicate requests. Simultaneous transitions produce one batch containing only the affected records. An ordinary source check is never triggered by the one-second display tick.
5. If Diablo is not foreground at the due time, defer network work. On return, recompute current state and perform at most one batch for all deferred events. Do not replay missed transitions or refetch on every Alt+Tab.
6. On sleep/resume, recalculate and discard obsolete scheduled callbacks. A clock jump invalidates pending deadlines; reconcile once when Diablo is active. Network reconnection may request one outstanding refresh, subject to backoff.

This yields roughly 4.7 small event-record reads per hour during uninterrupted foreground play: two Helltide boundary checks per hour, about 2.4 Legion checks, and about 0.29 boss checks. It is an estimate of this policy, not measured app traffic; initial loads and failed-request retries are additional.

The proposed first source adapter uses the three public timer records consumed by Helltides.com's own client. Plain HTTPS GETs to the `world_boss`, `helltide`, and `legion` records succeeded during research. The site exposes read replicas in its public page configuration. Keep endpoints in one adapter rather than spreading URLs through the app; retrieve only those small records, never the database root.

The separate `/api/schedule` request returned a Cloudflare challenge. It is not required for this design. The timer feeds are an observed website interface, not a guaranteed versioned public API, so schema validation and cached operation remain necessary.

## Handle stale data without repeated requests

Validate timestamps, ordering, expected record types, and plausible duration before accepting updates. Preserve the last valid record if a response fails validation or returns an older occurrence. Ignore names and locations in responses.

Save the timing anchors and last successful check for each event. On an unsuccessful refresh, continue calculated timers and prefix extrapolated values with `~`; expose the last successful sync in settings, without adding a fourth timer row. With no boss/Legion anchor, show an em dash until a valid record arrives. Helltide can still use the documented UTC cycle, marked as estimated.

Retry pending failed reads with capped exponential backoff, beginning at 60 seconds and growing to 30 minutes. Honor `Retry-After`, avoid parallel attempts for one record, and suppress retries while Diablo is not foreground. A transition must not reset the failure backoff. This is failure recovery, not normal periodic synchronization.

## Start automatically and respect Task Manager

Deliver a portable executable with settings stored beside it in a writable directory. On its first normal launch, register a stable, named per-user startup entry pointing to the fully quoted executable path. It then starts at Windows sign-in and idles until Diablo is active. No startup toggle is needed in the app.

The entry must appear under Task Manager > Startup apps with a recognizable name and icon. Respect disabling there: never modify Windows' disabled-state metadata or remove/recreate the entry to re-enable it. Store whether startup registration already occurred; do not silently re-create an entry the user later removes. If the user moves the portable folder and manually runs the app again, update an existing entry's path while preserving its disabled state.

A single-instance guard prevents duplicate overlays. A tray menu provides settings and exit. The first normal executable launch registers startup; tests and offscreen smoke checks do not.

## Native implementation and verification

Preferred implementation: a C++ Win32 executable using native window drawing, WinHTTP for HTTPS, and a message-loop coordinator. This small UI does not require an embedded browser or an always-running scripting interpreter. Keep window tracking, event calculations, source parsing, request scheduling, and settings separate enough to verify independently.

The build must produce a portable executable from included source and a documented build command. Resource consumption must be measured in the resulting app; no memory or CPU figures are promised from the mockup.

Required checks before delivery:

- Boundary calculations before, at, and after Helltide phases and boss/Legion spawns; UTC, DST, sleep/resume, and clock changes.
- No ordinary refresh before a scheduled transition; exactly one refresh per due event after its delay; no duplicate work after Alt+Tab or resume; proper retry suppression and backoff.
- Valid, unavailable, malformed, older, and changed-schedule responses; cached operation without a connection.
- Native visibility and input behavior over Diablo, collapse/expand and dragging, saved appearance, multi-monitor/DPI behavior where available.
- First-launch startup registration, Task Manager listing, disabled-state preservation, single instance, and executable relocation handling.
- Idle/active/hidden CPU and memory observations plus HTTP request counts during a representative run. Report what was actually exercised.

## Sources

- [Blizzard patch notes: Legion and World Boss intervals](https://news.blizzard.com/en-us/article/24092662/diablo-iv-patch-notes-1-0-1-2), Endgame Activities.
- [Blizzard patch notes: Helltide hourly cycle](https://news.blizzard.com/en-us/article/24140806/diablo-iv-patch-notes-1-3-1-5), End-Game Activities.
- [Helltides tracker and timing FAQ](https://helltides.com/).
- [Helltides World Boss schedule](https://helltides.com/worldboss).
- [Microsoft: SetWinEventHook](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook).
- [Microsoft: Configure startup applications](https://support.microsoft.com/en-us/windows/experience/startup-boot/configure-startup-applications-in-windows).

Research snapshots and website-client excerpts are retained in the task's working files. The live timing comparisons above verify adjacent advertised timestamps, not an observation of every recurrence or encounter in the game.
