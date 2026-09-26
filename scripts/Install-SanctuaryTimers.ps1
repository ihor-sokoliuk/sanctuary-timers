#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$Version = 'latest',
    [string]$InstallDirectory = (Join-Path $env:LOCALAPPDATA 'Programs\SanctuaryTimers'),
    [string]$MigrateFrom,
    [switch]$NoStart,
    [switch]$RepairStartup,
    [switch]$Uninstall
)

function Get-ReleaseSelection {
    param($Release)
    if($Release.draft -or $Release.prerelease -or $Release.tag_name -notmatch '^v(\d+\.\d+\.\d+)$'){throw 'Expected a stable versioned release'}
    $number=$Matches[1];$name="SanctuaryTimers-$number-win-x64.zip"
    $zip=@($Release.assets | Where-Object name -eq $name);$sums=@($Release.assets | Where-Object name -eq 'SHA256SUMS.txt')
    if($zip.Count -ne 1 -or $sums.Count -ne 1){throw 'Release assets are missing or ambiguous'}
    $base="https://github.com/ihor-sokoliuk/sanctuary-timers/releases/download/v$number/"
    if($zip[0].browser_download_url -cne ($base+$name) -or $sums[0].browser_download_url -cne ($base+'SHA256SUMS.txt')){throw 'Unexpected release asset origin'}
    [pscustomobject]@{Version=$number;Name=$name;PackageUrl=$zip[0].browser_download_url;ManifestUrl=$sums[0].browser_download_url}
}
function Assert-PackageHash {
    param([string]$Package,[string]$Manifest,[string]$Name)
    $entries=@($Manifest -split '\r?\n' | Where-Object {$_ -match ('^([a-fA-F0-9]{64}) [ *]'+[regex]::Escape($Name)+'$')})
    if($entries.Count -ne 1){throw 'Checksum entry is missing or ambiguous'}
    if((Get-FileHash -LiteralPath $Package -Algorithm SHA256).Hash -ine $entries[0].Substring(0,64)){throw 'SHA-256 mismatch; nothing installed'}
}
function Get-SafePackageEntries {
    param([string]$Package,[string]$Destination)
    Add-Type -AssemblyName System.IO.Compression,System.IO.Compression.FileSystem
    $root=[IO.Path]::GetFullPath($Destination).TrimEnd('\')+'\'
    $zip=[IO.Compression.ZipFile]::OpenRead($Package);$seen=@{};$files=New-Object 'Collections.Generic.List[string]';[long]$total=0
    try {
        foreach($entry in $zip.Entries){
            $name=$entry.FullName.Replace('/','\')
            if(!$name -or $name.Contains(':') -or [IO.Path]::IsPathRooted($name) -or $name -match '(^|\\)\.\.(\\|$)'){throw 'Unsafe archive path'}
            $target=[IO.Path]::GetFullPath((Join-Path $Destination $name))
            if(!$target.StartsWith($root,[StringComparison]::OrdinalIgnoreCase)){throw 'Archive path escapes staging'}
            if($seen.ContainsKey($target)){throw 'Duplicate archive path'};$seen[$target]=$true
            if($name -match '(^|\\)(settings\.ini|events\.ini|diagnostics\.log|status\.txt|install\.json)$'){throw 'Archive contains private runtime state'}
            $total+=$entry.Length;if($total -gt 64MB -or $zip.Entries.Count -gt 200){throw 'Archive exceeds package limits'}
            if(!$name.EndsWith('\')){$files.Add($name)}
        }
        if(!$files.Contains('SanctuaryTimers.exe') -or !$files.Contains('LICENSE')){throw 'Archive is not a Sanctuary Timers package'}
        return $files.ToArray()
    } finally { $zip.Dispose() }
}
function Get-StartupAction {
    param([bool]$Registered,[bool]$Exists,[bool]$SamePath,[bool]$Repair=$false)
    if(!$Exists){if($Registered -and !$Repair){return 'None'};return 'Create'}
    if($SamePath){return 'None'};return 'Update'
}
function Initialize-RegistryKey {
    param([string]$Path)
    if(!(Test-Path -LiteralPath $Path)){New-Item -Path $Path | Out-Null}
}
function Get-RegistryValue {
    param([string]$Path,[string]$Name)
    if(!(Test-Path -LiteralPath $Path)){return $null}
    $key=Get-Item -LiteralPath $Path -ErrorAction Stop
    return $key.GetValue($Name,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
}
function Invoke-WindowsUserRegistry {
    param([string]$Method,[string]$SubKey,[hashtable]$Values=@{},[switch]$AllowMissing)
    # A packaged host can expose a private HKCU view. Ask Windows' out-of-process
    # registry provider for this user's real hive; never fall back to that private view.
    $arguments=@{hDefKey=[uint32]2147483651;sSubKeyName=([Security.Principal.WindowsIdentity]::GetCurrent().User.Value+'\'+$SubKey)}
    foreach($name in $Values.Keys){$arguments[$name]=$Values[$name]}
    $result=Invoke-CimMethod -Namespace root/default -ClassName StdRegProv -MethodName $Method -Arguments $arguments -OperationTimeoutSec 10 -ErrorAction Stop
    if($result.ReturnValue -ne 0 -and !($AllowMissing -and $result.ReturnValue -eq 2)){throw "Windows registry $Method failed ($($result.ReturnValue)); startup was not verified"}
    return $result
}
function Get-WindowsRegistryString {
    param([string]$SubKey,[string]$Name)
    $values=Invoke-WindowsUserRegistry 'EnumValues' $SubKey -AllowMissing
    if($values.ReturnValue -eq 2){return $null}
    for($index=0;$index -lt $values.sNames.Count;$index++){
        if($values.sNames[$index] -ieq $Name){
            if($values.Types[$index] -ne 1){throw "Unexpected registry type for $Name"}
            return (Invoke-WindowsUserRegistry 'GetStringValue' $SubKey @{sValueName=$Name}).sValue
        }
    }
    return $null
}
function Set-WindowsRegistryString {
    param([string]$SubKey,[string]$Name,[string]$Value)
    Invoke-WindowsUserRegistry 'CreateKey' $SubKey | Out-Null
    Invoke-WindowsUserRegistry 'SetStringValue' $SubKey @{sValueName=$Name;sValue=$Value} | Out-Null
    if((Get-WindowsRegistryString $SubKey $Name) -cne $Value){throw "Windows registry readback failed for $Name"}
}
function Remove-WindowsRegistryStringIfOwned {
    param([string]$SubKey,[string]$Name,[string]$Expected)
    if((Get-WindowsRegistryString $SubKey $Name) -ieq $Expected){
        Invoke-WindowsUserRegistry 'DeleteValue' $SubKey @{sValueName=$Name} | Out-Null
        if($null -ne (Get-WindowsRegistryString $SubKey $Name)){throw "Windows registry removal failed for $Name"}
    }
}
function Register-WindowsStartup {
    param([string]$Executable,[bool]$Registered,[bool]$Repair)
    $key='Software\Microsoft\Windows\CurrentVersion\Run';$name='Sanctuary Timers'
    $old=Get-WindowsRegistryString $key $name;$command='"'+$Executable+'"'
    $action=Get-StartupAction $Registered ($null -ne $old) ($old -ieq $command) $Repair
    if($action -ne 'None'){Set-WindowsRegistryString $key $name $command}
    # Do not touch StartupApproved. Even explicit repair preserves Task Manager disablement.
    return $action
}
function Get-InstallationState {
    param([string]$Directory)
    $marker=Join-Path $Directory 'install.json'
    if(Test-Path -LiteralPath $marker){
        $prior=Get-Content -LiteralPath $marker -Raw | ConvertFrom-Json
        if($prior.Product -ne 'SanctuaryTimers' -or $prior.Directory -ine $Directory){throw 'Installation marker mismatch'}
        if($prior.PSObject.Properties['Installed'] -and !$prior.Installed){return 'Retained'}
        return 'Installed'
    }
    if((Test-Path -LiteralPath $Directory) -and @(Get-ChildItem -LiteralPath $Directory -Force).Count){throw 'Install directory is not empty and is not managed by this installer'}
    return 'Empty'
}
function Save-UninstalledState {
    param([string]$Directory,[string]$InstalledVersion)
    @{Product='SanctuaryTimers';Version=$InstalledVersion;Installed=$false;Directory=$Directory;Files=@()} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Directory 'install.json') -Encoding UTF8
}
function Restore-PreviousExecutable {
    [CmdletBinding()]
    param([string]$Executable,[string]$Backup,[scriptblock]$Stop = {param($exe) Stop-OwnedOverlay @($exe)})
    try {& $Stop $Executable;Copy-Item -LiteralPath $Backup -Destination $Executable -Force;return $true}
    catch {Write-Warning "Rollback could not complete. The previous executable is preserved at $Backup. $_";return $false}
}
function Get-OverlayProcesses {
    $session=(Get-Process -Id $PID).SessionId
    @(Get-CimInstance Win32_Process -Filter "Name='SanctuaryTimers.exe'" | Where-Object SessionId -eq $session)
}
function Invoke-OverlayCommand {
    param([string]$Executable,[string]$Command)
    # Windows PowerShell 5.1 Start-Process appends a space to the raw command tail.
    $info=New-Object Diagnostics.ProcessStartInfo
    $info.FileName=$Executable;$info.Arguments=$Command;$info.UseShellExecute=$false
    $info.CreateNoWindow=$true;$info.WindowStyle=[Diagnostics.ProcessWindowStyle]::Hidden
    $probe=[Diagnostics.Process]::Start($info)
    try {if(!$probe.WaitForExit(10000)){throw 'Overlay control command timed out'};return $probe.ExitCode}
    finally {$probe.Dispose()}
}
function Stop-OwnedOverlay {
    param([string[]]$AllowedPaths)
    foreach($app in @(Get-OverlayProcesses)){
        if(!$app.ExecutablePath -or $AllowedPaths -inotcontains $app.ExecutablePath){throw 'Another copy of Sanctuary Timers is running; exit it before installing'}
        $process=Get-Process -Id $app.ProcessId -ErrorAction Stop
        $quit=Invoke-OverlayCommand $app.ExecutablePath '--quit'
        if($quit -ne 0 -or !$process.WaitForExit(10000)){throw 'Overlay did not exit; files have not been replaced'}
    }
}
function Get-LaunchTaskName { 'Sanctuary Timers - '+[Security.Principal.WindowsIdentity]::GetCurrent().User.Value }
function Register-LaunchTask {
    param([string]$Executable)
    $service=New-Object -ComObject 'Schedule.Service';$service.Connect();$root=$service.GetFolder('\');$name=Get-LaunchTaskName
    $existing=$null;try{$existing=$root.GetTask($name)}catch{if($_.Exception.HResult -ne -2147024894){throw}}
    if($existing -and $existing.Definition.Actions.Item(1).Path -ine $Executable){throw 'An existing launch task belongs to another installation'}
    $definition=$service.NewTask(0);$identity=[Security.Principal.WindowsIdentity]::GetCurrent()
    $definition.RegistrationInfo.Author=$identity.Name
    $definition.RegistrationInfo.Description='On-demand independent launch for Sanctuary Timers. Automatic startup is controlled by the Task Manager Run entry.'
    $definition.Principal.UserId=$identity.User.Value;$definition.Principal.LogonType=3;$definition.Principal.RunLevel=0
    $definition.Settings.Enabled=$true;$definition.Settings.AllowDemandStart=$true;$definition.Settings.MultipleInstances=2
    $definition.Settings.ExecutionTimeLimit='PT0S';$definition.Settings.DisallowStartIfOnBatteries=$false;$definition.Settings.StopIfGoingOnBatteries=$false
    $definition.Settings.AllowHardTerminate=$false;$definition.Settings.StartWhenAvailable=$false
    $action=$definition.Actions.Create(0);$action.Path=$Executable;$action.WorkingDirectory=Split-Path $Executable -Parent
    $root.RegisterTaskDefinition($name,$definition,6,$identity.User.Value,$null,3,$null)
}
function Start-InstalledOverlay {
    param($Task,[string]$Executable)
    $instance=$Task.Run($null)
    $deadline=[DateTime]::UtcNow.AddSeconds(20)
    do {
        $found=@(Get-OverlayProcesses | Where-Object ExecutablePath -IEQ $Executable)
        if($found.Count -eq 1){
            $status=Join-Path (Split-Path $Executable -Parent) 'status.txt'
            if(Test-Path -LiteralPath $status){Remove-Item -LiteralPath $status -Force}
            $probe=Invoke-OverlayCommand $Executable '--status'
            if($probe -eq 0){
                for($attempt=0;$attempt -lt 20;$attempt++){if(Test-Path -LiteralPath $status){return $found[0].ProcessId};Start-Sleep -Milliseconds 100}
            }
        }
        Start-Sleep -Milliseconds 250
    }while([DateTime]::UtcNow -lt $deadline)
    throw "Windows did not start a responsive overlay. Task result: $($Task.LastTaskResult)"
}
function Invoke-Uninstall {
    param([string]$Directory)
    $marker=Join-Path $Directory 'install.json'
    if(!(Test-Path -LiteralPath $marker)){throw 'No managed installation found'}
    $installation=Get-Content -LiteralPath $marker -Raw | ConvertFrom-Json
    if($installation.Product -ne 'SanctuaryTimers' -or $installation.Directory -ine $Directory){throw 'Installation marker does not match this directory'}
    $exe=Join-Path $Directory 'SanctuaryTimers.exe';Stop-OwnedOverlay @($exe)
    $service=New-Object -ComObject 'Schedule.Service';$service.Connect();$root=$service.GetFolder('\');$name=Get-LaunchTaskName
    $task=$null;try{$task=$root.GetTask($name)}catch{if($_.Exception.HResult -ne -2147024894){throw}}
    if($task -and $task.Definition.Actions.Item(1).Path -ieq $exe){$root.DeleteTask($name,0)}
    Remove-WindowsRegistryStringIfOwned 'Software\Microsoft\Windows\CurrentVersion\Run' 'Sanctuary Timers' ('"'+$exe+'"')
    # Remove a matching private entry left by an older installer as well.
    $run='HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
    if((Get-RegistryValue $run 'Sanctuary Timers') -eq ('"'+$exe+'"')){Remove-ItemProperty -LiteralPath $run -Name 'Sanctuary Timers'}
    $uninstallKey='HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\SanctuaryTimers'
    if((Get-RegistryValue $uninstallKey InstallLocation) -ieq $Directory){Remove-Item -LiteralPath $uninstallKey}
    $shortcut=Join-Path ([Environment]::GetFolderPath('Programs')) 'Sanctuary Timers.lnk'
    if(Test-Path -LiteralPath $shortcut){$link=(New-Object -ComObject WScript.Shell).CreateShortcut($shortcut);if($link.TargetPath -ieq $exe){Remove-Item -LiteralPath $shortcut -Force}}
    $prefix=$Directory.TrimEnd('\')+'\'
    foreach($file in $installation.Files){$target=[IO.Path]::GetFullPath((Join-Path $Directory $file));if(!$target.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($target) -in @('settings.ini','events.ini')){throw 'Unsafe uninstall file list'};if(Test-Path -LiteralPath $target -PathType Leaf){Remove-Item -LiteralPath $target -Force}}
    Save-UninstalledState $Directory $installation.Version
    Write-Host "Uninstalled. Your preferences and cache remain in $Directory"
}
function Invoke-Install {
    param([string]$RequestedVersion,[string]$Directory,[string]$PreviousDirectory,[bool]$SkipStart,[bool]$Repair=$false)
    if(![Environment]::Is64BitOperatingSystem){throw 'Windows x64 is required'}
    if($RequestedVersion -ne 'latest' -and $RequestedVersion -notmatch '^v?\d+\.\d+\.\d+$'){throw 'Use latest or a version such as 0.1.1'}
    $uri='https://api.github.com/repos/ihor-sokoliuk/sanctuary-timers/releases/'
    if($RequestedVersion -eq 'latest'){$uri+='latest'}else{$uri+='tags/v'+$RequestedVersion.TrimStart('v')}
    [Net.ServicePointManager]::SecurityProtocol=[Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    $release=Get-ReleaseSelection (Invoke-RestMethod -Uri $uri -Headers @{'User-Agent'='SanctuaryTimers-Installer'} -TimeoutSec 30)
    $staging=Join-Path ([IO.Path]::GetTempPath()) ('SanctuaryInstall-'+[guid]::NewGuid().ToString('N'))
    [void][IO.Directory]::CreateDirectory($staging)
    try {
        Write-Host "Downloading Sanctuary Timers $($release.Version)..."
        $package=Join-Path $staging $release.Name;$manifest=Join-Path $staging 'SHA256SUMS.txt';$expanded=Join-Path $staging 'package'
        Invoke-WebRequest -UseBasicParsing -Uri $release.PackageUrl -OutFile $package -TimeoutSec 120
        Invoke-WebRequest -UseBasicParsing -Uri $release.ManifestUrl -OutFile $manifest -TimeoutSec 30
        Assert-PackageHash $package ([IO.File]::ReadAllText($manifest)) $release.Name
        $files=@(Get-SafePackageEntries $package $expanded)
        [IO.Compression.ZipFile]::ExtractToDirectory($package,$expanded)
        $newExe=Join-Path $expanded 'SanctuaryTimers.exe'
        if((Get-Item -LiteralPath $newExe).VersionInfo.ProductName -ne 'Sanctuary Timers' -or (Get-Item -LiteralPath $newExe).VersionInfo.FileVersion -ne $release.Version){throw 'Executable identity/version does not match the release'}
        $exe=Join-Path $Directory 'SanctuaryTimers.exe';$marker=Join-Path $Directory 'install.json';$wasInstalled=(Get-InstallationState $Directory) -eq 'Installed'
        $allowed=@($exe)
        if($PreviousDirectory){$PreviousDirectory=[IO.Path]::GetFullPath($PreviousDirectory);$oldExe=Join-Path $PreviousDirectory 'SanctuaryTimers.exe';if(!(Test-Path -LiteralPath $oldExe) -or (Get-Item -LiteralPath $oldExe).VersionInfo.ProductName -ne 'Sanctuary Timers'){throw 'Migration source is not a Sanctuary Timers installation'};$allowed+=$oldExe}
        # Prepare the independent launcher before stopping the working copy or changing startup.
        $task=Register-LaunchTask $exe
        Stop-OwnedOverlay $allowed
        [void][IO.Directory]::CreateDirectory($Directory)
        $backup=Join-Path $Directory 'SanctuaryTimers.previous.exe';if(Test-Path -LiteralPath $exe){Copy-Item -LiteralPath $exe -Destination $backup -Force}
        try {
            foreach($file in $files){$target=Join-Path $Directory $file;[void][IO.Directory]::CreateDirectory((Split-Path $target -Parent));Copy-Item -LiteralPath (Join-Path $expanded $file) -Destination $target -Force}
            if($PreviousDirectory){foreach($state in @('settings.ini','events.ini')){$from=Join-Path $PreviousDirectory $state;$to=Join-Path $Directory $state;if((Test-Path -LiteralPath $from) -and !(Test-Path -LiteralPath $to)){Copy-Item -LiteralPath $from -Destination $to}}}
            $action=Register-WindowsStartup $exe $wasInstalled $Repair
            $shortcut=Join-Path ([Environment]::GetFolderPath('Programs')) 'Sanctuary Timers.lnk'
            $link=(New-Object -ComObject WScript.Shell).CreateShortcut($shortcut);$link.TargetPath=$exe;$link.WorkingDirectory=$Directory;$link.IconLocation=$exe+',0';$link.Save()
            $uninstallKey='HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\SanctuaryTimers';Initialize-RegistryKey $uninstallKey
            $shell=Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\powershell.exe'
            $uninstallCommand='"'+$shell+'" -NoProfile -ExecutionPolicy Bypass -File "'+(Join-Path $Directory 'Install-SanctuaryTimers.ps1')+'" -Uninstall -InstallDirectory "'+$Directory+'"'
            foreach($pair in @{DisplayName='Sanctuary Timers';DisplayVersion=$release.Version;Publisher='Sanctuary Timers';InstallLocation=$Directory;DisplayIcon=$exe;UninstallString=$uninstallCommand;URLInfoAbout='https://github.com/ihor-sokoliuk/sanctuary-timers'}.GetEnumerator()){New-ItemProperty -LiteralPath $uninstallKey -Name $pair.Key -Value $pair.Value -PropertyType String -Force | Out-Null}
            @{Product='SanctuaryTimers';Version=$release.Version;Installed=$true;Directory=$Directory;Files=$files;Task=(Get-LaunchTaskName);Sha256=(Get-FileHash -LiteralPath $exe).Hash} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $marker -Encoding UTF8
            $running=$null;if(!$SkipStart){$running=Start-InstalledOverlay $task $exe}
            [pscustomobject]@{Version=$release.Version;Directory=$Directory;ProcessId=$running;StartupAction=$action;LaunchTask=(Get-LaunchTaskName)}
        } catch {
            $installFailure=$_
            if(Test-Path -LiteralPath $backup){
                if(Restore-PreviousExecutable $exe $backup){
                    Write-Warning 'Previous executable restored. Installation metadata may have been updated; rerun the installer to complete setup.'
                    if(!$SkipStart){try{Start-InstalledOverlay $task $exe | Out-Null}catch{Write-Warning "Previous executable could not be restarted: $_"}}
                }
            }
            throw $installFailure
        }
    } finally {
        $full=[IO.Path]::GetFullPath($staging);$temp=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
        if(!$full.StartsWith($temp,[StringComparison]::OrdinalIgnoreCase) -or (Split-Path $full -Leaf) -notlike 'SanctuaryInstall-*'){throw 'Unsafe staging cleanup path'}
        Remove-Item -LiteralPath $full -Recurse -Force
    }
}

if ($MyInvocation.InvocationName -eq '.') { return }
$ErrorActionPreference='Stop'
$InstallDirectory=[IO.Path]::GetFullPath($InstallDirectory).TrimEnd('\')
if($InstallDirectory -eq [IO.Path]::GetPathRoot($InstallDirectory).TrimEnd('\') -or $InstallDirectory.StartsWith($env:WINDIR,[StringComparison]::OrdinalIgnoreCase)){throw 'Choose a dedicated application directory'}
if($Uninstall){Invoke-Uninstall $InstallDirectory}else{Invoke-Install $Version $InstallDirectory $MigrateFrom ([bool]$NoStart) ([bool]$RepairStartup)}
