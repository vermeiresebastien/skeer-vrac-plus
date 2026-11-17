<#
    Refresh the repository-root launch/ folder with this configuration's build output
    and restamp the portable package. Run as the `launch` CMake target, or directly:

        powershell -File native/package-launch.ps1 -Version 1.0.0

    Nothing in launch/ is deleted until its replacement has been staged and hash-matched, and the
    zip is written last, so a failure leaves the previous export where it was instead of handing
    back an empty deliverables folder in a tracked directory.
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
$InstallNote = Join-Path $NativeDir 'Package\INSTALL.txt'
$NoticesSrc = Join-Path $NativeDir 'Package\THIRD-PARTY-NOTICES.txt'
$ReadmeSrc = Join-Path $NativeDir 'README.md'
# AGPL-3.0 conveys obligations with the object code, so the licence text travels in the archive
# rather than only in the repository, and the installer ships because INSTALL.txt tells the receiver
# to run it. An archive that names a script it does not contain is a package that cannot be installed.
$LicenceSrc = Join-Path $RepoRoot 'LICENSE'
$ShotsDir = Join-Path $NativeDir 'shots'
$StartPs1 = Join-Path $LaunchDir 'start.ps1'
$StartBat = Join-Path $LaunchDir 'start.bat'

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

# Relative path -> SHA-256 for one candidate export: the standalone plus every file of the bundle.
# Matching only the module DLL, as this script used to, certifies a bundle that dropped a Resources
# file on the way - which is a plugin the host cannot load.
function Get-ExportHashMap([string]$ExePath, [string]$BundlePath) {
    $map = @{}
    $map['sliceforge.exe'] = (Get-FileHash -LiteralPath $ExePath -Algorithm SHA256).Hash
    $rootFull = (Get-Item -LiteralPath $BundlePath).FullName.TrimEnd('\')
    Get-ChildItem -LiteralPath $BundlePath -Recurse -File -Force | ForEach-Object {
        $rel = $_.FullName.Substring($rootFull.Length).TrimStart('\').ToLowerInvariant().Replace('\', '/')
        $map['sliceforge.vst3/' + $rel] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    }
    return $map
}

# Differences as messages, none when the two maps describe the same bytes. A key present on only one
# side is a file the copy skipped or invented, so it counts as a difference too.
function Compare-ExportHashMaps([hashtable]$Expected, [hashtable]$Actual) {
    $problems = @()
    foreach ($key in $Expected.Keys) {
        if (-not $Actual.ContainsKey($key)) { $problems += "$key is missing" }
        elseif ($Actual[$key] -ne $Expected[$key]) { $problems += "$key has a different hash" }
    }
    foreach ($key in $Actual.Keys) {
        if (-not $Expected.ContainsKey($key)) { $problems += "$key is not in the build output" }
    }
    return $problems
}

function Assert-SameBytes([object]$Problems, [string]$What) {
    # Compare-ExportHashMaps writes nothing when the trees match, so $null here means clean. The
    # obvious `$list = if (...) { @() } else { ... }` is the trap: an empty array coming out of a
    # block is no output at all, the assignment lands as $null, and .Count on it is a hard error.
    if ($null -eq $Problems) { return }
    $list = @($Problems) | Select-Object -First 6
    throw ("$What :: " + (($list | ForEach-Object { "$_" }) -join '; '))
}

# This README is an archive member, so a picture it links that the archive does not carry is a broken
# image in the customer's copy - the same defect this script already refuses for a start script or a
# licence file. The shot list is therefore read OUT OF the README rather than written beside it, so the
# two cannot drift: the only remaining disagreement is a link to a file that is not on disk, and that
# throws here instead of shipping a placeholder for someone else to find.
function Get-ShippedShotList([string]$ReadmePath, [string]$ShotsDirPath) {
    if (-not (Test-Path -LiteralPath $ShotsDirPath -PathType Container)) {
        throw "Missing $ShotsDirPath - the README links pictures out of it, so there is no package without it."
    }
    $rootFull = (Get-Item -LiteralPath $ShotsDirPath).FullName.TrimEnd('\')
    $raw = Get-Content -LiteralPath $ReadmePath -Raw
    $names = @([regex]::Matches($raw, '!\[[^\]]*\]\((shots/[A-Za-z0-9._-]+\.png)\)') |
        ForEach-Object { $_.Groups[1].Value } | Select-Object -Unique | Sort-Object)
    $list = @()
    foreach ($name in $names) {
        # The character class admits no separator, so nothing after 'shots/' can be a directory. The
        # prefix test is still there because a member that resolves outside shots/ is not a screenshot.
        $path = Join-Path $rootFull (Split-Path -Leaf $name)
        if (-not $path.StartsWith($rootFull + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "'$name' in $ReadmePath does not name a file inside $rootFull"
        }
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "$ReadmePath links $name, but $path is not on disk - the package would ship a broken image."
        }
        $list += @{ Name = $name; Path = $path }
    }
    # The other half of the invariant: a link this step does not recognise would still be a broken
    # picture for the customer, so an uncarried relative image link fails the export rather than the
    # reader. Only an absolute URL is excused, because that target is not the archive's to supply.
    foreach ($link in @([regex]::Matches($raw, '!\[[^\]]*\]\(([^)]+)\)') |
            ForEach-Object { $_.Groups[1].Value } | Select-Object -Unique)) {
        if ($link -match '^[A-Za-z][A-Za-z0-9+.-]*:') { continue }
        if ($names -notcontains $link) {
            throw "$ReadmePath links '$link', which this step does not carry - every picture in a shipped README needs an archive member."
        }
    }
    return $list
}

$Staging = Join-Path $env:TEMP ('sliceforge-launch-' + [guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Force $Staging | Out-Null
    Copy-Item -LiteralPath $ExeSrc -Destination (Join-Path $Staging 'SliceForge.exe') -Force
    # The whole bundle folder, not the inner DLL: a VST3 module cannot resolve its Resources.
    Copy-Item -LiteralPath $BundleSrc -Destination (Join-Path $Staging 'SliceForge.vst3') -Recurse -Force

    # A skipped copy leaves an old mtime on a file that exported correctly, so every comparison here
    # is by hash and never by timestamp.
    $wanted = Get-ExportHashMap $ExeSrc $BundleSrc
    $staged = Get-ExportHashMap (Join-Path $Staging 'SliceForge.exe') (Join-Path $Staging 'SliceForge.vst3')
    Assert-SameBytes (Compare-ExportHashMaps $wanted $staged) "the staged copy does not match $Artefacts"

    # The previous export goes only now, with its replacement already proven in staging, so a failed
    # copy cannot leave a tracked deliverables folder empty.
    foreach ($stale in @((Join-Path $LaunchDir 'SliceForge.exe'), (Join-Path $LaunchDir 'SliceForge.vst3'))) {
        if (Test-Path -LiteralPath $stale) { Remove-Item -LiteralPath $stale -Recurse -Force }
    }
    # The previous zip is NOT deleted here. It used to be, which meant any failure in the packing or
    # verification below handed back a tracked deliverables folder with no portable package in it at
    # all - the exact outcome the header of this file promises cannot happen. The verified archive is
    # moved over the old one at the end, and Move-Item -Force is the replacement.

    Move-Item -LiteralPath (Join-Path $Staging 'SliceForge.exe') -Destination (Join-Path $LaunchDir 'SliceForge.exe')
    Move-Item -LiteralPath (Join-Path $Staging 'SliceForge.vst3') -Destination (Join-Path $LaunchDir 'SliceForge.vst3')

    # A move between volumes is a copy again, so what landed is matched against the build output
    # rather than trusted because the staged tree was clean.
    $landed = Get-ExportHashMap (Join-Path $LaunchDir 'SliceForge.exe') (Join-Path $LaunchDir 'SliceForge.vst3')
    Assert-SameBytes (Compare-ExportHashMaps $wanted $landed) "launch/ does not match $Artefacts after the move"

    $Zip = Join-Path $LaunchDir ("SliceForge-$Version-Windows-x64.zip")
    $ZipStaging = Join-Path $env:TEMP ('sliceforge-package-' + [guid]::NewGuid().ToString('N') + '.zip')
    try {
        # Packed from launch/, so the archive and the folder cannot disagree, and packed last: every
        # byte inside it has already been verified sitting there.
        #
        # Written member-by-member instead of with Compress-Archive, because Compress-Archive puts a
        # BACKSLASH in every nested member name in both the central directory and the local headers,
        # which is not what the ZIP spec (APPNOTE 4.4.17, "the path stored ... using '/' ") and
        # POSIX require of a portable archive. The module is a folder and the screenshots live in a
        # subdirectory, so nested members are exactly the ones that were affected, and a host or an
        # unzipper that keys on the spec separator finds no
        # bundle at all. CreateEntry takes the name verbatim, so the separator here is a decision
        # rather than a side effect, and it is '/'. Directory rows are not written: an archive whose
        # members all name real files cannot disagree with the folder it was packed from.
        Add-Type -AssemblyName System.IO.Compression
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $LaunchBundle = Join-Path $LaunchDir 'SliceForge.vst3'
        $bundleRootFull = (Get-Item -LiteralPath $LaunchBundle).FullName.TrimEnd('\')
        $Shots = Get-ShippedShotList $ReadmeSrc $ShotsDir
        $members = @()
        $members += @{ Name = 'SliceForge.exe'; Path = (Join-Path $LaunchDir 'SliceForge.exe') }
        $members += @{ Name = 'INSTALL.txt';    Path = $InstallNote }
        $members += @{ Name = 'README.md';      Path = $ReadmeSrc }
        $members += @{ Name = 'LICENSE';                   Path = $LicenceSrc }
        $members += @{ Name = 'THIRD-PARTY-NOTICES.txt';   Path = $NoticesSrc }
        $members += @{ Name = 'start.ps1';                 Path = $StartPs1 }
        $members += @{ Name = 'start.bat';                 Path = $StartBat }
        # The pictures the shipped README links travel with it, under the same relative name, so the
        # archive renders on its own and every PNG is hashed back exactly like a binary member.
        foreach ($shot in $Shots) { $members += @{ Name = $shot.Name; Path = $shot.Path } }
        Get-ChildItem -LiteralPath $LaunchBundle -Recurse -File -Force | ForEach-Object {
            $rel = $_.FullName.Substring($bundleRootFull.Length).TrimStart('\').Replace('\', '/')
            $members += @{ Name = ('SliceForge.vst3/' + $rel); Path = $_.FullName }
        }
        $zipFs = [System.IO.File]::Open($ZipStaging, [System.IO.FileMode]::Create)
        try {
            $archiveWriter = New-Object System.IO.Compression.ZipArchive(
                $zipFs, [System.IO.Compression.ZipArchiveMode]::Create)
            try {
                foreach ($member in $members) {
                    $entry = $archiveWriter.CreateEntry($member.Name,
                        [System.IO.Compression.CompressionLevel]::Optimal)
                    $src = [System.IO.File]::OpenRead($member.Path)
                    try {
                        $dst = $entry.Open()
                        try { $src.CopyTo($dst) } finally { $dst.Dispose() }
                    } finally { $src.Dispose() }
                }
            } finally { $archiveWriter.Dispose() }
        } finally { $zipFs.Dispose() }

        # An archive holds independently compressed members: nothing so far proved this one contains
        # these bytes rather than a truncated or stale copy of them, which is what a user finds out
        # only after installing from it.
        $zipWanted = @{}
        foreach ($pair in $wanted.GetEnumerator()) { $zipWanted[$pair.Key] = $pair.Value }
        $zipWanted['install.txt'] = (Get-FileHash -LiteralPath $InstallNote -Algorithm SHA256).Hash
        $zipWanted['readme.md'] = (Get-FileHash -LiteralPath $ReadmeSrc -Algorithm SHA256).Hash
        $zipWanted['license'] = (Get-FileHash -LiteralPath $LicenceSrc -Algorithm SHA256).Hash
        $zipWanted['third-party-notices.txt'] = (Get-FileHash -LiteralPath $NoticesSrc -Algorithm SHA256).Hash
        $zipWanted['start.ps1'] = (Get-FileHash -LiteralPath $StartPs1 -Algorithm SHA256).Hash
        $zipWanted['start.bat'] = (Get-FileHash -LiteralPath $StartBat -Algorithm SHA256).Hash
        foreach ($shot in $Shots) {
            $zipWanted[$shot.Name.ToLowerInvariant()] = (Get-FileHash -LiteralPath $shot.Path -Algorithm SHA256).Hash
        }

        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $found = @{}
        $badSeparators = @()
        $BundlePrefix = 'sliceforge.vst3/'
        $archive = [System.IO.Compression.ZipFile]::OpenRead($ZipStaging)
        try {
            foreach ($entry in $archive.Entries) {
                # Byte-wise, and deliberately NOT Replace('\','/'): normalising the separator here is
                # what let an archive full of backslashed member names pass this gate for as long as it
                # took the folder offline. A member name is data the receiving system reads, so the
                # verifier has to look at the bytes actually stored, and a backslash is a failure in
                # its own right, reported before the hashes are compared.
                if ($entry.FullName.Contains('\')) { $badSeparators += $entry.FullName }
                $key = $entry.FullName.ToLowerInvariant()
                # Compress-Archive writes a member for every directory as well as every file, so a
                # member is judged by the path it maps back to: the directory rows are not files,
                # and anything that maps outside these roots stays in the comparison and is
                # reported as a member the export never produced.
                $onDisk = $null
                if ($key -eq 'sliceforge.exe') { $onDisk = Join-Path $LaunchDir 'SliceForge.exe' }
                elseif ($key -eq 'install.txt') { $onDisk = $InstallNote }
                elseif ($key -eq 'readme.md') { $onDisk = $ReadmeSrc }
                elseif ($key -eq 'license') { $onDisk = $LicenceSrc }
                elseif ($key -eq 'third-party-notices.txt') { $onDisk = $NoticesSrc }
                elseif ($key -eq 'start.ps1') { $onDisk = $StartPs1 }
                elseif ($key -eq 'start.bat') { $onDisk = $StartBat }
                elseif ($key.StartsWith('shots/')) {
                    $onDisk = Join-Path $ShotsDir ($key.Substring('shots/'.Length).Replace('/', '\'))
                }
                elseif ($key.StartsWith($BundlePrefix)) {
                    $onDisk = Join-Path $LaunchDir ($key.Replace('/', '\'))
                }
                if ($null -ne $onDisk -and (Test-Path -LiteralPath $onDisk -PathType Container)) { continue }
                $stream = $entry.Open()
                try { $hashed = ([System.Security.Cryptography.SHA256]::Create()).ComputeHash($stream) }
                finally { $stream.Dispose() }
                $found[$key] = ([System.BitConverter]::ToString($hashed) -replace '-', '').ToUpperInvariant()
            }
        }
        finally { $archive.Dispose() }

        if ($badSeparators.Count -gt 0) {
            throw ("$ZipStaging stores " + $badSeparators.Count + ' member name(s) with a backslash: ' +
                   (($badSeparators | Select-Object -First 6 | ForEach-Object { "$_" }) -join '; '))
        }
        Assert-SameBytes (Compare-ExportHashMaps $zipWanted $found) "$ZipStaging does not contain the exported build"
        Move-Item -LiteralPath $ZipStaging -Destination $Zip -Force
    }
    finally {
        if (Test-Path -LiteralPath $ZipStaging) { Remove-Item -LiteralPath $ZipStaging -Force }
    }
}
finally {
    if (Test-Path -LiteralPath $Staging) { Remove-Item -LiteralPath $Staging -Recurse -Force }
}

Write-Host "launch/ refreshed ($Config, v$Version)"
Get-ChildItem -LiteralPath $LaunchDir | ForEach-Object {
    $size = if ($_.PSIsContainer) { 'folder' } else { '{0:N1} MB' -f ($_.Length / 1MB) }
    Write-Host ("  {0,-36} {1}" -f $_.Name, $size)
}
