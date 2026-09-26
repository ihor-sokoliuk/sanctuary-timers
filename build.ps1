param([string]$Zig = 'zig', [switch]$TestsOnly)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
  New-Item -ItemType Directory -Path build -Force | Out-Null
  $common = @('-target','x86_64-windows-gnu','-std=c++20','-O2','-g','-static','-DUNICODE','-D_UNICODE','-DNOMINMAX','-Isrc','-Wall','-Wextra')
  & $Zig c++ @common src/core.cpp tests/core_tests.cpp -o build/core_tests.exe
  if($LASTEXITCODE) { throw 'Test compilation failed' }
  & ./build/core_tests.exe
  if($LASTEXITCODE) { throw 'Tests failed; application build blocked' }
  & $Zig c++ @common src/clock.cpp tests/clock_tests.cpp -o build/clock_tests.exe
  if($LASTEXITCODE) { throw 'Clock test compilation failed' }
  & ./build/clock_tests.exe
  if($LASTEXITCODE) { throw 'Clock tests failed; application build blocked' }
  & $Zig c++ @common src/core.cpp src/clock.cpp src/platform.cpp src/clock_network.cpp tests/platform_tests.cpp -lwinhttp -luser32 -ladvapi32 -lws2_32 -o build/platform_tests.exe
  if($LASTEXITCODE) { throw 'Platform test compilation failed' }
  & ./build/platform_tests.exe
  if($LASTEXITCODE) { throw 'Platform tests failed; application build blocked' }
  & $Zig rc /fo build/app.res resources/app.rc
  if($LASTEXITCODE) { throw 'Application resource compilation failed' }
  & $Zig c++ @common src/core.cpp src/render.cpp tests/render_tests.cpp build/app.res -ld2d1 -ldwrite -lgdi32 -luser32 -lole32 -o build/render_tests.exe
  if($LASTEXITCODE) { throw 'Renderer test compilation failed' }
  & ./build/render_tests.exe
  if($LASTEXITCODE) { throw 'Renderer tests failed; application build blocked' }
  if(!$TestsOnly) {
    & $Zig c++ @common -municode '-Wl,--subsystem,windows' src/core.cpp src/clock.cpp src/platform.cpp src/clock_network.cpp src/render.cpp src/main.cpp build/app.res -lwinhttp -ld2d1 -ldwrite -lgdi32 -luser32 -lshell32 -ladvapi32 -lole32 -lshlwapi -lws2_32 -o build/SanctuaryTimers.exe
    if($LASTEXITCODE) { throw 'Application compilation failed' }
  }
} finally { Pop-Location }
