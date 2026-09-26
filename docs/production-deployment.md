# Production deployment

The user authorized publishing a production release and installing it on this Windows machine independently of Codex. Preserve the current appearance, timing cache and Task Manager startup control. Do not synthesize input or restart Diablo.

## Delivery plan

1. Add a PowerShell 5.1-compatible installer with pinned release selection, SHA-256 validation, safe extraction, preference migration and bounded shutdown of the existing overlay. Test pure installer decisions before installation.
2. Install under `%LOCALAPPDATA%\Programs\SanctuaryTimers`, add a Start menu entry and Apps uninstall registration, and preserve the current named Run entry and its disabled-state metadata.
3. Use an on-demand, current-user interactive Task Scheduler task to start the app outside Codex's process tree. Set no execution time limit and allow battery operation. Give the task no automatic triggers so it cannot bypass Task Manager startup disablement.
4. Package version 0.1.2, run native and installer tests, review the deployment change, push and verify Windows CI, then publish a GitHub release with the portable ZIP, installer and SHA-256 manifest.
5. Install by downloading the published release, migrate this user's existing settings, and verify executable identity, parent process, task configuration, startup path, visible overlay and fresh event records. End the installer and confirm the same app process remains alive.

## Scope rulings

- Publication and installation are explicitly authorized in the current request; no additional approval checkpoint is needed.
- Startup uses the existing Run mechanism, keeping Task Manager authoritative. The on-demand task supplies an independent launch now; it is not a second automatic startup path.
- Do not sign out, restart Windows, or close Codex to test lifetime. Prove independent Windows launch and survival after the installer exits, and report the remaining sign-in/reboot verification limit.
- Keep a backup of the prior executable and preserve unrelated files. A failed install must report its actual state, rather than claiming deployment from task-registration success alone.

## Result

The native suites (29 core cases plus platform and rendering suites) and 13 installer cases pass locally. Installer tests also pass under Windows PowerShell 5.1. The on-demand task registered successfully with an interactive user token, no triggers and no runtime limit.

Read-only review caught registry key replacement, retained-state reinstall, and failed rollback backup handling. Conditional key initialization now preserves unrelated Run values; uninstallation retains a managed ownership marker; rollback keeps its backup outside temporary staging. Dedicated regression cases cover these behaviors. No installer modified production files or startup registration before these fixes.

The first published installer (0.1.1) stopped before replacing any application files: Windows PowerShell 5.1 `Start-Process` appended a trailing space to its control argument, which the native application rejected. Version 0.1.2 uses a hidden direct process launch to preserve the exact argument. A real executable regression fixture reproduces the original failure and verifies the corrected command and exit code; this brings the installer suite to 14 cases.

Version [0.1.2](https://github.com/ihor-sokoliuk/sanctuary-timers/releases/tag/v0.1.2) was published from commit `892a94ddefcd9a44778135423c271de4c1fe01e5`. [Windows CI](https://github.com/ihor-sokoliuk/sanctuary-timers/actions/runs/36204853848) passed the native suites and all 14 installer cases. The release assets came from that CI run. The installer was downloaded from the published release, matched against the tested artifact, and executed using Windows PowerShell 5.1.

Installation and passive live verification on September 25, 2026 confirmed:

- The installed executable under `%LOCALAPPDATA%\Programs\SanctuaryTimers` reports version 0.1.2 and matches the executable inside the published ZIP. Its SHA-256 is `A2C3AC579F2B7CA110586C2EDD075A280871D647E20F15F8C938FA3360C8CEA2`.
- Exactly one overlay process is running. Its parent is the Windows service host containing the Schedule service, with `services.exe` above it; no installer or Codex process is in its ancestry. The same application process remained responsive after the installer exited.
- The launch task is running with an interactive user token, no automatic triggers and no execution time limit. The quoted Run entry points to the installed executable. Start menu and Installed apps entries resolve to this installation.
- The settings file is byte-for-byte unchanged. All six unrelated Run values and the app's Task Manager startup-state metadata are unchanged. Existing cached records were migrated, and the previous portable folder remains available.
- After a natural return to Diablo, both overlay windows became visible with the expected no-activate/topmost styles, 361-by-133 physical-pixel panel at 150% scale, and contained controls. Diablo remained foreground during this observation. Each event record refreshed successfully once; all three records were verified, with no HTTP failures or drawing errors.

No input was synthesized and no window was deliberately focused. Windows sign-in/reboot and closing Codex were not performed; startup registration and independent process ownership were verified without interrupting the active session. Version 0.1.1 is marked superseded on its release page.

## Daily clock update: 0.1.3

The user authorized daily clock synchronization, remote publication and local deployment with autostart. Version [0.1.3](https://github.com/ihor-sokoliuk/sanctuary-timers/releases/tag/v0.1.3) was released from `a7448863945c9cfb6d9cfcd15f61c09976f3edfb`, following a focused read-only review with no material findings and successful [Windows CI](https://github.com/ihor-sokoliuk/sanctuary-timers/actions/runs/36246860907). CI passed four native suites (including 29 core and 11 clock cases) and 14 installer cases.

The published installer updated the managed installation using Windows PowerShell 5.1. The installed executable exactly matches the CI release ZIP, SHA-256 `1046E58B700941E530D966180C3F9B8FA6E3FC7DE4E9602D3A7A0061B653E997`. Version 0.1.2 remains as the previous executable for rollback.

The new Windows-owned process completed its first clock check while Diablo was not foreground: one successful SNTP exchange, -376 ms correction, 42 ms round trip, and next check scheduled for 86,400 seconds. Event fetches remained deferred. A later status request confirmed the same process, still only one clock request, and a decreasing daily deadline. Preferences, the quoted autostart path, Task Manager metadata and all unrelated startup values were preserved. The installer exited before these checks; the application remained responsive. No Windows time settings or foreground window were changed.

Appearance text/layout was verified by offscreen rendering at 150% scaling. This deployment did not force a game focus change, sign-in/reboot, or a 24-hour live wait; injected-time tests cover the daily boundary and sleep catch-up.

## Cached event colors: 0.1.4

The user gave standing authorization to publish and install future changes to this project automatically. The repository's `AGENTS.md` records that instruction and the existing verification and foreground-preservation requirements.

Version [0.1.4](https://github.com/ihor-sokoliuk/sanctuary-timers/releases/tag/v0.1.4) was published from `b6ae413c34ab7ae8681b2c9d07760f8a72be9079` after successful [Windows CI](https://github.com/ihor-sokoliuk/sanctuary-timers/actions/runs/36248135735). The four native suites, including the new event-color pixel checks, and Windows PowerShell 5.1 installer suite passed. Release assets came from that exact CI run; archive paths, contents and SHA-256 checks were verified before publication.

The published installer updated the managed local copy using Windows PowerShell 5.1. The installed version reports 0.1.4 and exactly matches the executable inside the release ZIP: SHA-256 `A4976D197702EBD9C053704707396B7752B32CD629E4EB5F94F5572C1EAA2EF3`. The previous 0.1.3 executable remains intact for rollback.

Passive checks confirmed one responsive overlay process owned by the Windows Schedule service, surviving the completed installer with no Codex or PowerShell ancestor. Preferences and event cache were byte-for-byte unchanged; the quoted Run path, Task Manager startup metadata and all seven unrelated Run values were preserved. The on-demand task still has no automatic triggers or execution time limit. Startup, Start menu and Installed apps registration passed verification.

The first daily clock check succeeded with a -404 ms correction and 41 ms round trip. A later fresh status showed the same process, one clock request, no clock failures and a decreasing 24-hour deadline. Diablo was not foreground, so event requests and painting remained suspended. The foreground process was unchanged across installation; no input was synthesized. Colors were verified with offscreen rendering in both local and CI builds, not by forcing the live game into the foreground. No sign-in/reboot or full-day wait was performed.

## Launch detection recovery: 0.1.5

Version [0.1.5](https://github.com/ihor-sokoliuk/sanctuary-timers/releases/tag/v0.1.5) was published from `e79905bc9196d6f99c673f0d3947689f2e106805` after focused read-only review found no material issues and [Windows CI](https://github.com/ihor-sokoliuk/sanctuary-timers/actions/runs/36250549439) passed all five native suites and the installer suite. Local tests covered 29 core cases, 11 clock cases, 11 tracking cases, platform persistence and offscreen rendering. Publication used that exact CI artifact with verified archive contents and checksums.

The downloaded published installer updated the managed copy using Windows PowerShell 5.1. The installed executable reports 0.1.5 and matches the release ZIP's SHA-256 `44EF9BBE2FCF42A010B7FCA54E94AA8258149C825896B54C1E3BEFA10BD749F9`. The previous 0.1.4 executable remains intact for rollback. Preferences, Task Manager startup metadata, the quoted Run command and all seven unrelated Run values were preserved; Start menu, Installed apps and the on-demand launch task remained correct.

The overlay appeared over the already-running Diablo window immediately after Windows started it, with the game retaining foreground across installation. Passive inspection confirmed both overlay windows were visible at the saved 345-by-129 physical-pixel size and 150% scaling, retaining topmost/no-activate styles and click-through display. A later fresh status, about 50 seconds after launch, confirmed the same independent Windows-owned process, all discovery hooks registered, `game-ready`, no detection error or pending retry, and no drawing errors. The live bitmap showed the expected bold text and event colors.

The first clock synchronization succeeded with a -458 ms correction and 15 ms round trip. Each event record refreshed successfully once; later natural focus changes caused no additional event reads. No input was synthesized, and Diablo was not restarted or deliberately focused. The tests exercise launch-transition recovery, but a new end-to-end Diablo launch has not been observed since this update. Windows sign-in/reboot and a full daily clock interval were not exercised.
