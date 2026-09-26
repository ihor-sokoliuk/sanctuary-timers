#Requires -Version 5.1
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\scripts\Install-SanctuaryTimers.ps1')
Add-Type -AssemblyName System.IO.Compression,System.IO.Compression.FileSystem
$passed = 0; $failed = 0
function Check($Condition) { if (!$Condition) { throw 'Assertion failed' } }
function Reject([scriptblock]$Body) { $rejected = $false; try { & $Body | Out-Null } catch { $rejected = $true }; Check $rejected }
function Test([string]$Name,[scriptblock]$Body) { try { & $Body; $script:passed++; Write-Host "PASS $Name" } catch { $script:failed++; Write-Host "FAIL ${Name}: $_" } }
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('SanctuaryInstallerTests-' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
function Make-Zip([string[]]$Names) {
    $zipPath = Join-Path $fixture ([guid]::NewGuid().ToString('N')+'.zip')
    $zip = [IO.Compression.ZipFile]::Open($zipPath,[IO.Compression.ZipArchiveMode]::Create)
    try { foreach($name in $Names) { $entry=$zip.CreateEntry($name); $writer=New-Object IO.StreamWriter($entry.Open()); $writer.Write('fixture'); $writer.Dispose() } } finally { $zip.Dispose() }
    return $zipPath
}
try {
    Test 'hidden control launch preserves exact native arguments and exit code' {
        $controlFixture=Join-Path $fixture 'ControlArguments.exe'
        Add-Type -OutputAssembly $controlFixture -OutputType WindowsApplication @'
using System;
public class ControlArguments {
    public static int Main() { return Environment.CommandLine.EndsWith(" --status",StringComparison.Ordinal) ? 42 : 2; }
}
'@
        Check ((Invoke-OverlayCommand $controlFixture '--status') -eq 42)
    }
    $release = @{ tag_name='v0.1.1'; draft=$false; prerelease=$false; assets=@(
        @{name='SanctuaryTimers-0.1.1-win-x64.zip';browser_download_url='https://github.com/ihor-sokoliuk/sanctuary-timers/releases/download/v0.1.1/SanctuaryTimers-0.1.1-win-x64.zip'},
        @{name='SHA256SUMS.txt';browser_download_url='https://github.com/ihor-sokoliuk/sanctuary-timers/releases/download/v0.1.1/SHA256SUMS.txt'}) }
    Test 'select the exact stable release assets' { $selected=Get-ReleaseSelection $release; Check ($selected.Version -eq '0.1.1'); Check ($selected.Name -eq 'SanctuaryTimers-0.1.1-win-x64.zip') }
    Test 'reject draft prerelease or missing archive' { $bad=$release.Clone();$bad.draft=$true;Reject {Get-ReleaseSelection $bad};$bad=$release.Clone();$bad.prerelease=$true;Reject {Get-ReleaseSelection $bad};$bad=$release.Clone();$bad.assets=@();Reject {Get-ReleaseSelection $bad} }
    Test 'reject assets from another origin' { $bad=$release.Clone();$bad.assets=@(@{name='SanctuaryTimers-0.1.1-win-x64.zip';browser_download_url='https://example.com/app.zip'},$release.assets[1]);Reject {Get-ReleaseSelection $bad} }
    $package=Make-Zip @('SanctuaryTimers.exe','LICENSE','licenses/font.txt')
    $digest=(Get-FileHash -LiteralPath $package -Algorithm SHA256).Hash
    Test 'accept matching hash and exact file name' { Assert-PackageHash $package "$digest  app.zip" 'app.zip' }
    Test 'reject hash mismatch missing entry and duplicate entry' { Reject {Assert-PackageHash $package (('0'*64)+'  app.zip') 'app.zip'};Reject {Assert-PackageHash $package "$digest  another.zip" 'app.zip'};Reject {Assert-PackageHash $package "$digest  app.zip`n$digest  app.zip" 'app.zip'} }
    Test 'accept ordinary package entries' { $entries=@(Get-SafePackageEntries $package (Join-Path $fixture 'stage'));Check ($entries.Count -eq 3);Check ($entries -contains 'SanctuaryTimers.exe') }
    Test 'reject traversal absolute paths ADS and protected runtime files' { foreach($entry in @('../escape.exe','/absolute.exe','C:\escape.exe','file.txt:payload','settings.ini','events.ini','status.txt','diagnostics.log')) { $unsafe=Make-Zip @('SanctuaryTimers.exe','LICENSE',$entry);Reject {Get-SafePackageEntries $unsafe (Join-Path $fixture 'stage')} } }
    Test 'reject case insensitive duplicate entries' { $unsafe=Make-Zip @('SanctuaryTimers.exe','SANCTUARYTIMERS.EXE','LICENSE');Reject {Get-SafePackageEntries $unsafe (Join-Path $fixture 'stage')} }
    Test 'reject archives without the executable' { $unsafe=Make-Zip @('README.md');Reject {Get-SafePackageEntries $unsafe (Join-Path $fixture 'stage')} }
    Test 'startup update preserves deliberate deletion' { Check ((Get-StartupAction $false $false $false) -eq 'Create');Check ((Get-StartupAction $true $false $false) -eq 'None');Check ((Get-StartupAction $true $true $false) -eq 'Update');Check ((Get-StartupAction $true $true $true) -eq 'None') }
    Test 'registry initialization preserves other startup values' {
        $key='HKCU:\Software\SanctuaryInstallerTests-'+[guid]::NewGuid().ToString('N')
        try {Check ($null -eq (Get-RegistryValue $key 'Missing'));Initialize-RegistryKey $key;Check ($null -eq (Get-RegistryValue $key 'Missing'));New-ItemProperty -LiteralPath $key -Name OtherApp -Value 'keep this' | Out-Null;Initialize-RegistryKey $key;Check ((Get-RegistryValue $key OtherApp) -eq 'keep this')}finally{if(Test-Path -LiteralPath $key){Remove-Item -LiteralPath $key}}
    }
    Test 'uninstall ownership state permits reinstall and preserves preferences' {
        $directory=Join-Path $fixture 'reinstall';[void][IO.Directory]::CreateDirectory($directory);Check ((Get-InstallationState $directory) -eq 'Empty')
        [IO.File]::WriteAllText((Join-Path $directory 'settings.ini'),'font=15');[IO.File]::WriteAllText((Join-Path $directory 'events.ini'),'cached')
        @{Product='SanctuaryTimers';Version='0.1.1';Installed=$true;Directory=$directory;Files=@('SanctuaryTimers.exe')} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $directory 'install.json')
        Check ((Get-InstallationState $directory) -eq 'Installed');Save-UninstalledState $directory '0.1.1';Check ((Get-InstallationState $directory) -eq 'Retained')
        Check ([IO.File]::ReadAllText((Join-Path $directory 'settings.ini')) -eq 'font=15');Check ([IO.File]::ReadAllText((Join-Path $directory 'events.ini')) -eq 'cached')
    }
    Test 'rollback retains backup when stopping the new process fails' {
        $exe=Join-Path $fixture 'new.exe';$backup=Join-Path $fixture 'previous.exe';[IO.File]::WriteAllText($exe,'new');[IO.File]::WriteAllText($backup,'previous')
        Check (!(Restore-PreviousExecutable $exe $backup {throw 'unresponsive'} -WarningAction SilentlyContinue));Check ([IO.File]::ReadAllText($backup) -eq 'previous');Check ([IO.File]::ReadAllText($exe) -eq 'new')
        Check (Restore-PreviousExecutable $exe $backup {});Check ([IO.File]::ReadAllText($exe) -eq 'previous')
    }
} finally {
    $full=[IO.Path]::GetFullPath($fixture);$temp=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if(!$full.StartsWith($temp,[StringComparison]::OrdinalIgnoreCase) -or (Split-Path $full -Leaf) -notlike 'SanctuaryInstallerTests-*'){throw 'Unsafe test cleanup path'}
    Remove-Item -LiteralPath $full -Recurse -Force
}
Write-Host "$passed passed, $failed failed"
if($failed){throw 'Installer tests failed'}
