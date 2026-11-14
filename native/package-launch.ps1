<#
    Refresh the repository-root launch/ folder with this configuration's build output
    and restamp the portable package. Run as the `launch` CMake target, or directly:

        powershell -File native/package-launch.ps1 -Version 1.0.0
#>
[CmdletBinding()]
param(
    [string]$Version = '1.0.0',
    [string]$BuildDir = '',
    [ValidateSet('Release', 'Debug', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Config = 'Release'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$NativeDir = $PSScriptRoot
$RepoRoot = Split-Path -Parent $NativeDir
if (-not $BuildDir) { $BuildDir = Join-Path $NativeDir 'build' }

$Artefacts = Join-Path $BuildDir ('SliceForge_artefacts\' + $Config)
$ExeSrc = Join-Path $Artefacts 'Standalone\SliceForge.exe'
$BundleSrc = Join-Path $Artefacts 'VST3\SliceForge.vst3'
$LaunchDir = Join-Path $RepoRoot 'launch'

if (-not (Test-Path -LiteralPath $ExeSrc)) {
    throw "Missing $ExeSrc - build SliceForge_Standalone first."
}
if (-not (Test-Path -LiteralPath (Join-Path $BundleSrc 'Contents\x86_64-win\SliceForge.vst3'))) {
    throw "Missing $BundleSrc - build SliceForge_VST3 first."
}

New-Item -ItemType Directory -Force $LaunchDir | Out-Null

# Windows refuses to overwrite a running exe, and the standalone does not autosave, so this step
# stops instead of killing his session.
$running = @(Get-Process -Name 'SliceForge' -ErrorAction SilentlyContinue)
if ($running.Count -gt 0) {
    throw ('SliceForge.exe is running (PID ' + (($running.Id | ForEach-Object { "$_" }) -join ', ') +
           ') - close it, then re-run. The export does not kill it for you.')
}

# Remove the previous export so a file the build no longer produces cannot survive in launch/.
foreach ($stale in @((Join-Path $LaunchDir 'SliceForge.exe'), (Join-Path $LaunchDir 'SliceForge.vst3'))) {
    if (Test-Path -LiteralPath $stale) { Remove-Item -LiteralPath $stale -Recurse -Force }
}
Get-ChildItem -LiteralPath $LaunchDir -Filter 'SliceForge-*-Windows-x64.zip' -File -ErrorAction SilentlyContinue |
    Remove-Item -Force

Copy-Item -LiteralPath $ExeSrc -Destination (Join-Path $LaunchDir 'SliceForge.exe') -Force

# The whole bundle folder, not the inner DLL: a VST3 module cannot resolve its Resources.
Copy-Item -LiteralPath $BundleSrc -Destination (Join-Path $LaunchDir 'SliceForge.vst3') -Recurse -Force

$Staging = Join-Path $env:TEMP ('sliceforge-launch-' + [guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Force $Staging | Out-Null
    Copy-Item -LiteralPath (Join-Path $LaunchDir 'SliceForge.exe') -Destination $Staging -Force
    Copy-Item -LiteralPath (Join-Path $LaunchDir 'SliceForge.vst3') -Destination $Staging -Recurse -Force
    Copy-Item -LiteralPath (Join-Path $NativeDir 'Package\INSTALL.txt') -Destination $Staging -Force
    Copy-Item -LiteralPath (Join-Path $NativeDir 'README.md') -Destination (Join-Path $Staging 'README.md') -Force

    $Zip = Join-Path $LaunchDir ("SliceForge-$Version-Windows-x64.zip")
    Compress-Archive -LiteralPath @((Join-Path $Staging 'SliceForge.exe'),
                                   (Join-Path $Staging 'SliceForge.vst3'),
                                   (Join-Path $Staging 'INSTALL.txt'),
                                   (Join-Path $Staging 'README.md')) `
                    -DestinationPath $Zip -CompressionLevel Optimal -Force
}
finally {
    if (Test-Path -LiteralPath $Staging) { Remove-Item -LiteralPath $Staging -Recurse -Force }
}

# A skipped copy leaves an old mtime on a file that exported correctly, so hash-match instead of
# trusting timestamps.
$Dlldst = Join-Path $LaunchDir 'SliceForge.vst3\Contents\x86_64-win\SliceForge.vst3'
$checks = @(@($ExeSrc, (Join-Path $LaunchDir 'SliceForge.exe')),
            @((Join-Path $BundleSrc 'Contents\x86_64-win\SliceForge.vst3'), $Dlldst))
foreach ($pair in $checks) {
    $a = (Get-FileHash -LiteralPath $pair[0] -Algorithm SHA256).Hash
    $b = (Get-FileHash -LiteralPath $pair[1] -Algorithm SHA256).Hash
    if ($a -ne $b) { throw "Export hash mismatch for $($pair[1]): $a != $b" }
}

Write-Host "launch/ refreshed ($Config, v$Version)"
Get-ChildItem -LiteralPath $LaunchDir | ForEach-Object {
    $size = if ($_.PSIsContainer) { 'folder' } else { '{0:N1} MB' -f ($_.Length / 1MB) }
    Write-Host ("  {0,-36} {1}" -f $_.Name, $size)
}
