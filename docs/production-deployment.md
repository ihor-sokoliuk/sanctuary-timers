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
