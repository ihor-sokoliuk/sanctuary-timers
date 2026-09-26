#Requires -Version 5.1
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Executable,[string]$OutputDirectory)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$binary=Get-Item -LiteralPath $Executable
$version=$binary.VersionInfo.FileVersion
if($binary.VersionInfo.ProductName -ne 'Sanctuary Timers' -or $version -notmatch '^\d+\.\d+\.\d+$'){throw 'Expected a versioned Sanctuary Timers executable'}
if(!$OutputDirectory){$OutputDirectory=Join-Path $repo 'release'}
$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory)
[void][IO.Directory]::CreateDirectory($OutputDirectory)
$stage=Join-Path ([IO.Path]::GetTempPath()) ('SanctuaryPackage-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($stage)
try {
    Copy-Item -LiteralPath $binary.FullName -Destination (Join-Path $stage 'SanctuaryTimers.exe')
    foreach($file in @('README.md','LICENSE','THIRD-PARTY-NOTICES.md')){Copy-Item -LiteralPath (Join-Path $repo $file) -Destination $stage}
    Copy-Item -LiteralPath (Join-Path $repo 'licenses'),(Join-Path $repo 'docs') -Destination $stage -Recurse
    $installer=Join-Path $PSScriptRoot 'Install-SanctuaryTimers.ps1'
    Copy-Item -LiteralPath $installer -Destination $stage
    Copy-Item -LiteralPath $installer -Destination $OutputDirectory -Force
    $zip=Join-Path $OutputDirectory "SanctuaryTimers-$version-win-x64.zip"
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -Force
    $hashes=foreach($file in @($zip,(Join-Path $OutputDirectory 'Install-SanctuaryTimers.ps1'))){((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant())+'  '+[IO.Path]::GetFileName($file)}
    $hashes | Set-Content -LiteralPath (Join-Path $OutputDirectory 'SHA256SUMS.txt') -Encoding ASCII
    Get-ChildItem -LiteralPath $OutputDirectory | Select-Object Name,Length
} finally {
    $full=[IO.Path]::GetFullPath($stage);$temp=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if(!$full.StartsWith($temp,[StringComparison]::OrdinalIgnoreCase) -or (Split-Path $full -Leaf) -notlike 'SanctuaryPackage-*'){throw 'Unsafe package cleanup path'}
    Remove-Item -LiteralPath $full -Recurse -Force
}
