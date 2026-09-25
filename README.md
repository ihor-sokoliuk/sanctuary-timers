# Sanctuary Timers

A compact native Windows overlay for Diablo IV's World Boss, Helltide and Legion timers.

## Play with the overlay

Place `SanctuaryTimers.exe` in a writable folder you intend to keep, then run it. Use Diablo IV in Windowed Fullscreen. The panel appears when Diablo is foreground and hides when another application takes focus.

The left strip contains collapse/expand, drag, and settings. The timer area lets clicks pass to the game. Settings open inside the panel: adjust text size, width, and background opacity with the minus/plus controls. Text stays opaque. Appearance and position are saved beside the executable.

There are no sounds, popups, flashing alerts, maps, or build tools. `Now` means the scheduled event started within the last minute; it does not claim a boss is still alive in your instance. Helltide switches between `ends` and `in` at its known boundaries. `~` marks estimated/offline data; an em dash means no trustworthy timing anchor is available.

## Start with Windows or exit

The first normal launch adds **Sanctuary Timers** under **Task Manager > Startup apps**. Disable it there to stop automatic startup. The app preserves that choice and does not recreate an entry you remove. If you move its folder, run the app once from the new location to update an existing startup path.

Right-click the tray icon to open appearance settings or exit. Starting it a second time does not create another panel. `SanctuaryTimers.exe --quit` also requests a graceful exit without changing focus.

## Timing and network behavior

The app fetches three small public records used by Helltides.com. It calculates countdowns locally, then checks the affected event 20 seconds after a known start/end. It does not refresh all events every five minutes. Requests are deferred while the game is not foreground. Failures retain cached anchors and back off from one minute to thirty minutes, honoring server retry delays.

Helltides follow the hourly 55-minute cycle. World Boss and Legion calculations use validated feed anchors and 210-minute/25-minute intervals. A changed source phase suspends extrapolation until reconfirmed. These are website feeds, not a guaranteed Blizzard API; changes can require an adapter update.

## Build and test

Windows 10/11 x64. No browser engine, Python, .NET, or installed runtime is required to use the built executable.

With a portable [Zig 0.15.2](https://ziglang.org/download/) C++ compiler:

```powershell
.\build.ps1 -Zig C:\tools\zig\zig.exe -TestsOnly
.\build.ps1 -Zig C:\tools\zig\zig.exe
```

The build script runs unit tests before producing `build/SanctuaryTimers.exe`. It never launches the overlay or registers startup. Alternatively use a Visual Studio C++ toolchain:

```powershell
cmake -S . -B build-msvc -A x64
cmake --build build-msvc --config Release
ctest --test-dir build-msvc -C Release --output-on-failure
```

## Implementation and privacy

The source uses C++20, Win32, Direct2D/DirectWrite and WinHTTP. Window focus/location changes arrive through out-of-context WinEvent notifications. The application does not read game memory, inject code, capture the game, or automate gameplay. There is no telemetry or update checker. `settings.ini`, `events.ini`, `diagnostics.log` and an on-demand `status.txt` remain beside the executable.

The display uses a click-through drawing surface and a tightly aligned input surface entirely within its bounds, so gameplay clicks can pass through to another process reliably. They appear as one panel and never activate themselves.

See [design](docs/design.md), [implementation plan](docs/implementation-plan.md) and [validation](docs/validation.md) for scope and measured verification. Resource usage must be measured; native code alone does not establish a performance claim.

## Attribution

Independent fan utility, not affiliated with Blizzard. Event data and schedule reference: [Helltides.com](https://helltides.com/). Existing projects, including Helltime and D4 World Boss Overlay, were researched as design references; their code and assets are not bundled. Source is MIT licensed. Compiler runtime license notices accompany binary distributions.
