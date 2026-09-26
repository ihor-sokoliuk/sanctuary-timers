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
    Test 'explicit startup repair recreates a missing registration' {
        Check ((Get-StartupAction $true $false $false $true) -eq 'Create')
        Check ((Get-StartupAction $true $true $true $true) -eq 'None')
    }
    Test 'Windows registry provider targets this user and rejects access errors' {
        Get-Command Invoke-WindowsUserRegistry -ErrorAction Stop | Out-Null
        $script:testRegistryCall=$null
        function Invoke-CimMethod {
            param($Namespace,$ClassName,$MethodName,$Arguments,$ErrorAction,$OperationTimeoutSec)
            $script:testRegistryCall=@{Namespace=$Namespace;ClassName=$ClassName;Arguments=$Arguments}
            [pscustomobject]@{ReturnValue=5}
        }
        Reject {Invoke-WindowsUserRegistry 'EnumValues' 'Software\Fixture' -AllowMissing}
        Check ($script:testRegistryCall.Namespace -eq 'root/default');Check ($script:testRegistryCall.ClassName -eq 'StdRegProv')
        Check ($script:testRegistryCall.Arguments.hDefKey -eq [uint32]2147483651)
        Check ($script:testRegistryCall.Arguments.sSubKeyName -eq ([Security.Principal.WindowsIdentity]::GetCurrent().User.Value+'\Software\Fixture'))
    }
    Test 'Windows registry provider propagates transport errors' {
        Get-Command Invoke-WindowsUserRegistry -ErrorAction Stop | Out-Null
        function Invoke-CimMethod {throw 'Provider unavailable'}
        Reject {Invoke-WindowsUserRegistry 'EnumValues' 'Software\Fixture' -AllowMissing}
    }
    Test 'Windows registry read failures cannot be mistaken for absent values' {
        Get-Command Get-WindowsRegistryString -ErrorAction Stop | Out-Null
        function Invoke-CimMethod { [pscustomobject]@{ReturnValue=1} }
        Reject {Get-WindowsRegistryString 'Software\Fixture' 'Missing'}
    }
    Test 'startup repair uses Windows state and preserves ordinary deletion' {
        $script:testStartup=$null;$script:testStartupWrites=0
        function Get-WindowsRegistryString {return $script:testStartup}
        function Set-WindowsRegistryString {param($Key,$Name,$Value);Check ($Name -eq 'Sanctuary Timers');$script:testStartup=$Value;$script:testStartupWrites++}
        Check ((Register-WindowsStartup 'C:\Program Files\SanctuaryTimers.exe' $true $false) -eq 'None')
        Check ($script:testStartupWrites -eq 0)
        Check ((Register-WindowsStartup 'C:\Program Files\SanctuaryTimers.exe' $true $true) -eq 'Create')
        Check ($script:testStartup -ceq '"C:\Program Files\SanctuaryTimers.exe"')
        Check ((Register-WindowsStartup 'C:\Program Files\SanctuaryTimers.exe' $true $false) -eq 'None')
        Check ((Register-WindowsStartup 'C:\New\SanctuaryTimers.exe' $true $false) -eq 'Update')
        Check ($script:testStartupWrites -eq 2)
    }
    Test 'Windows registry write requires successful independent readback' {
        Get-Command Set-WindowsRegistryString -ErrorAction Stop | Out-Null
        function Invoke-WindowsUserRegistry { [pscustomobject]@{ReturnValue=0} }
        function Get-WindowsRegistryString {return 'stale command'}
        Reject {Set-WindowsRegistryString 'Software\Fixture' 'Sanctuary Timers' 'new command'}
    }
    Test 'real Windows registry roundtrip preserves unrelated values and owned removal' {
        $key='Software\SanctuaryInstallerTests-'+[guid]::NewGuid().ToString('N')
        try {
            Check ($null -eq (Get-WindowsRegistryString $key 'Missing'))
            Set-WindowsRegistryString $key 'OtherApp' 'keep this'
            Set-WindowsRegistryString $key 'Sanctuary Timers' '"C:\Old\SanctuaryTimers.exe"'
            Set-WindowsRegistryString $key 'Sanctuary Timers' '"C:\New\SanctuaryTimers.exe"'
            # An independent Windows provider, not PowerShell's potentially isolated HKCU view.
            $sid=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value
            $read=Invoke-CimMethod -Namespace root/default -ClassName StdRegProv -MethodName GetStringValue -Arguments @{hDefKey=[uint32]2147483651;sSubKeyName=($sid+'\'+$key);sValueName='Sanctuary Timers'}
            Check ($read.ReturnValue -eq 0);Check ($read.sValue -ceq '"C:\New\SanctuaryTimers.exe"')
            Remove-WindowsRegistryStringIfOwned $key 'Sanctuary Timers' '"C:\Wrong\SanctuaryTimers.exe"'
            Check ((Get-WindowsRegistryString $key 'Sanctuary Timers') -ceq $read.sValue)
            Remove-WindowsRegistryStringIfOwned $key 'Sanctuary Timers' $read.sValue
            Check ($null -eq (Get-WindowsRegistryString $key 'Sanctuary Timers'))
            Check ((Get-WindowsRegistryString $key 'OtherApp') -ceq 'keep this')
        } finally {Invoke-WindowsUserRegistry 'DeleteKey' $key -AllowMissing | Out-Null}
    }
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
