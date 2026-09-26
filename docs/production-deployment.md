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

Corrected release publication and downloaded-build installation verification pending.
