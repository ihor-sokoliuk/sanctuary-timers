param([string]$Zig = 'zig', [switch]$TestsOnly)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
  New-Item -ItemType Directory -Path build -Force | Out-Null
  $common = @('-std=c++20','-O2','-g','-static','-DUNICODE','-D_UNICODE','-DNOMINMAX','-Isrc','-Wall','-Wextra')
  & $Zig c++ @common src/core.cpp tests/core_tests.cpp -o build/core_tests.exe
  if($LASTEXITCODE) { throw 'Test compilation failed' }
  & ./build/core_tests.exe
  if($LASTEXITCODE) { throw 'Tests failed; application build blocked' }
  if(!$TestsOnly) {
    & $Zig c++ @common -municode -mwindows src/core.cpp src/platform.cpp src/main.cpp -lwinhttp -ld2d1 -ldwrite -lgdi32 -luser32 -lshell32 -ladvapi32 -lole32 -lshlwapi -o build/SanctuaryTimers.exe
    if($LASTEXITCODE) { throw 'Application compilation failed' }
  }
} finally { Pop-Location }
