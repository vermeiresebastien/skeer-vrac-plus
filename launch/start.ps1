<#
    SKEER SliceForge launcher.

    1. Refreshes the installed SliceForge.vst3 bundle from this folder so a new build reaches the DAW.
    2. Starts the standalone instrument.
    3. In a repo checkout only: serves the browser edition (reuses a dev server that is already
       listening) and opens it. A portable package has no src/, so this step reports that instead of
       opening a URL nothing is serving.

        start.bat                            all three
        start.ps1 -DryRun                    show the plan, change nothing
        start.ps1 -InstallOnly               step 1 only, and exit non-zero if it did not land
        start.ps1 -Vst3Dir "..."             install into a non-default VST3 folder
#>
[CmdletBinding()]
param(
    [switch]$DryRun,
    [switch]$InstallOnly,
    # Internal: marks the UAC-elevated copy of this script, so it installs instead of asking
    # again, and never starts the standalone or the dev server under an admin token.
    [switch]$ElevatedInstall,
    [string]$Vst3Dir = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$LaunchDir = $PSScriptRoot
$RepoRoot = Split-Path -Parent $LaunchDir
$Exe = Join-Path $LaunchDir 'SliceForge.exe'
$Bundle = Join-Path $LaunchDir 'SliceForge.vst3'
$Url = 'http://127.0.0.1:5173/'

# Two layouts run this script: launch/ inside a checkout (browser edition available) and the root of
# an extracted portable package (no src/, no node_modules, and no npm to ask for them). The test is
# the two things step 3 actually needs, not a marker file that could disagree with them.
$IsRepoCheckout = (Test-Path -LiteralPath (Join-Path $RepoRoot 'src')) -and
                  (Test-Path -LiteralPath (Join-Path $RepoRoot 'node_modules'))

function Write-Step([string]$Message) { Write-Host ('>> ' + $Message) -ForegroundColor Cyan }
function Skip([string]$Message) { if ($DryRun) { Write-Host ('   [dry-run] ' + $Message); return $true } return $false }

# The install step is the only part that may need elevation, so it runs again in a UAC-elevated
# copy of this script with -ElevatedInstall. That copy has its own console, which closes when the
# copy finishes, so its lines go to a log the calling process prints.
$LogPath = Join-Path $env:TEMP 'SliceForge-vst3-install.log'
$ChildLog = if ($ElevatedInstall) { $LogPath } else { $null }
function Say([string]$Message, [string]$Colour = 'Gray') {
    Write-Host ('   ' + $Message) -ForegroundColor $Colour
    if ($ChildLog) { Add-Content -LiteralPath $ChildLog -Value $Message }
}
function Test-IsAdmin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    return ([Security.Principal.WindowsPrincipal]$id).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Test-Port([int]$Port) {
    $client = New-Object System.Net.Sockets.TcpClient
    try {
        $async = $client.BeginConnect('127.0.0.1', $Port, $null, $null)
        return $async.AsyncWaitHandle.WaitOne(300) -and $client.Connected
    }
    finally { $client.Close() }
}

# ---- 1. VST3 install ---------------------------------------------------------------
Write-Step 'VST3 install'
if ($ElevatedInstall) { Say "elevated child started, administrator token = $(Test-IsAdmin)" }

# One destination, always: the system VST3 folder that every host scans by default. Installing
# beside it into Documents\VST3 or %LOCALAPPDATA% is what produced two entries per plugin and two
# copies that drifted apart after a rebuild, so a SliceForge bundle found in either is removed once
# the system copy is hash-verified. -Vst3Dir still overrides for a machine with a custom scan root.
$ModuleRel = Join-Path 'Contents' (Join-Path 'x86_64-win' 'SliceForge.vst3')
$OtherVst3Dirs = @(
    (Join-Path $env:USERPROFILE 'Documents\VST3'),
    (Join-Path $env:LOCALAPPDATA 'Programs\Common\VST3')
)

function Test-DirWritable([string]$Dir) {
    # A folder that does not exist yet can only be created by writing to its nearest real ancestor,
    # which is the probe that matters: %ProgramFiles%\Common Files\VST3 is missing on a machine that
    # has never had a machine-wide plugin installed, and answering "writable" without looking would
    # skip the elevation prompt and fail the copy instead.
    $cursor = $Dir
    while ($cursor -and -not (Test-Path -LiteralPath $cursor)) {
        $parent = Split-Path -Path $cursor -Parent
        if (-not $parent -or $parent -eq $cursor) { return $false }
        $cursor = $parent
    }
    if (-not $cursor) { return $false }
    $probe = Join-Path $cursor ('.sf-probe-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
    try { New-Item -ItemType Directory -Path $probe | Out-Null; Remove-Item -Path $probe -Recurse -Force; return $true }
    catch { return $false }
}

# Two spellings of one folder - 8.3 short name, a trailing slash, different case, a junction - are
# equal here. Comparing the raw strings is what would let the prune step retire the bundle that was
# installed a moment ago, because -Vst3Dir and the default can name the same directory differently.
function Get-CanonicalDir([string]$Dir) {
    $tail = @()
    $cursor = $Dir.TrimEnd('\', '/')
    while ($cursor -and -not (Test-Path -LiteralPath $cursor)) {
        $leaf = Split-Path -Path $cursor -Leaf
        if (-not $leaf) { break }
        $tail = @($leaf) + $tail
        $parent = Split-Path -Path $cursor -Parent
        if (-not $parent -or $parent -eq $cursor) { break }
        $cursor = $parent
    }
    $base = $cursor
    if ($base -and (Test-Path -LiteralPath $base)) {
        try { $base = (Get-Item -LiteralPath $base -Force).FullName } catch { }
    }
    else { try { $base = [System.IO.Path]::GetFullPath($Dir) } catch { $base = $Dir } }
    return (@($base) + $tail -join '\').TrimEnd('\', '/').ToLowerInvariant()
}

# Relative path -> SHA-256 for every file of a bundle. One hash of the module DLL certified a bundle
# that arrived without its Resources, which is a plugin the host lists and cannot load.
function Get-BundleHashMap([string]$BundlePath) {
    $map = @{}
    $rootFull = (Get-Item -LiteralPath $BundlePath).FullName.TrimEnd('\')
    Get-ChildItem -LiteralPath $BundlePath -Recurse -File -Force | ForEach-Object {
        $rel = $_.FullName.Substring($rootFull.Length).TrimStart('\').ToLowerInvariant().Replace('\', '/')
        $map[$rel] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    }
    return $map
}

function Test-SameBundle([hashtable]$A, [hashtable]$B) {
    if ($A.Count -ne $B.Count) { return $false }
    foreach ($key in $A.Keys) {
        if (-not $B.ContainsKey($key) -or $B[$key] -ne $A[$key]) { return $false }
    }
    return $true
}

# Staged copy, verified, then swapped: deleting the previous bundle before the new one is proven
# would leave the DAW with nothing if the copy failed halfway.
function Install-Vst3Bundle([string]$SourceBundle, [string]$Destination) {
    $dest = Join-Path $Destination 'SliceForge.vst3'
    $staging = Join-Path $Destination ('SliceForge.vst3.staging-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
    $wanted = Get-BundleHashMap $SourceBundle

    Copy-Item -LiteralPath $SourceBundle -Destination $staging -Recurse -Force
    if (-not (Test-SameBundle $wanted (Get-BundleHashMap $staging))) {
        Remove-Item -LiteralPath $staging -Recurse -Force
        throw 'the staged copy failed its hash check - nothing was replaced'
    }
    if (Test-Path -LiteralPath $dest) { Remove-Item -LiteralPath $dest -Recurse -Force }
    Move-Item -LiteralPath $staging -Destination $dest
    if (-not (Test-SameBundle $wanted (Get-BundleHashMap $dest))) {
        throw "the bundle in $dest does not match the one that was staged"
    }
    return $wanted
}

# A stray SliceForge is retired, not destroyed: only a bundle proven byte-identical to the installed
# one is a duplicate. Anything else is a build that exists nowhere else, and deleting it would take
# the last copy of it with the message "superseded".
function Remove-DuplicateBundles([string]$KeptDir, [hashtable]$Wanted) {
    $kept = Get-CanonicalDir $KeptDir
    $RetiredDir = Join-Path $env:LOCALAPPDATA 'SliceForge\retired'
    foreach ($other in @($OtherVst3Dirs | Where-Object { (Get-CanonicalDir $_) -ne $kept })) {
        $stale = Join-Path $other 'SliceForge.vst3'
        if (-not (Test-Path -LiteralPath $stale)) { continue }
        try {
            $staleMap = Get-BundleHashMap $stale
            if (Test-SameBundle $Wanted $staleMap) {
                Remove-Item -LiteralPath $stale -Recurse -Force
                Say "removed $stale (byte-identical to the installed bundle)"
            }
            else {
                $moduleKey = $ModuleRel.ToLowerInvariant().Replace('\', '/')
                $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
                $tag = if ($staleMap.ContainsKey($moduleKey)) { $staleMap[$moduleKey].Substring(0, 8) } else { 'unknown' }
                $park = Join-Path $RetiredDir ($stamp + '-' + $tag)
                New-Item -ItemType Directory -Force $park | Out-Null
                Move-Item -LiteralPath $stale -Destination (Join-Path $park 'SliceForge.vst3')
                Say "moved $stale to $park - it is a different build, not a duplicate" 'Yellow'
            }
        }
        catch { Say "left $stale alone ($($_.Exception.Message))" 'Yellow' }
    }
}

$BundleDll = Join-Path $Bundle $ModuleRel
if (-not (Test-Path -LiteralPath $BundleDll)) {
    Say "Missing $BundleDll - run: cmake --build native/build --config Release --target launch" 'Yellow'
    if ($InstallOnly) { exit 1 }
}
else {
    $target = if ($Vst3Dir) { $Vst3Dir } else { Join-Path $env:ProgramFiles 'Common Files\VST3' }
    $dest = Join-Path $target 'SliceForge.vst3'
    $WantMap = Get-BundleHashMap $Bundle
    $WantHash = $WantMap[$ModuleRel.ToLowerInvariant().Replace('\', '/')]
    $writable = Test-DirWritable $target
    $systemVst3 = Join-Path $env:ProgramFiles 'Common Files\VST3'

    # -Vst3Dir is an escape hatch for a host whose scan root really is elsewhere, not a licence to
    # keep a second copy: say out loud when the system folder still holds one, because that is the
    # duplicate-browser-entry state this script exists to prevent.
    if ((Get-CanonicalDir $target) -ne (Get-CanonicalDir $systemVst3)) {
        Say "installing to a non-default folder: $target" 'Yellow'
        if (Test-Path -LiteralPath (Join-Path $systemVst3 'SliceForge.vst3')) {
            Say "a copy still sits in $systemVst3 - the host will list SliceForge twice" 'Yellow'
        }
    }

    if ($DryRun) {
        Say "[dry-run] install $Bundle -> $dest$(if ($writable) { '' } else { '   (needs elevation)' })"
        foreach ($other in @($OtherVst3Dirs | Where-Object { (Get-CanonicalDir $_) -ne (Get-CanonicalDir $target) })) {
            $stale = Join-Path $other 'SliceForge.vst3'
            if (Test-Path -LiteralPath $stale) { Say "[dry-run] retire $stale (delete if identical, move aside if not)" }
        }
    }
    elseif (-not $writable) {
        if ($ElevatedInstall) {
            # Only reached when the prompt was declined, or this session cannot show one.
            Say "still no write access to $target as $env:USERNAME - elevation did not happen" 'Red'
        }
        else {
            Say "$target needs administrator rights - approving one UAC prompt for the install step only" 'Yellow'
            $ps = Join-Path $PSHOME 'powershell.exe'
            if (-not (Test-Path -LiteralPath $ps)) { $ps = Join-Path $PSHOME 'pwsh.exe' }
            Remove-Item -LiteralPath $LogPath -Force -ErrorAction SilentlyContinue
            # Every path argument is quoted by hand: Windows PowerShell 5.1 joins -ArgumentList
            # with spaces without escaping elements that contain them, so an unquoted
            # "C:\Program Files\..." silently arrives as three arguments and the child dies on a
            # parameter error before it can report anything.
            $quoted = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$PSCommandPath`"",
                        '-InstallOnly', '-ElevatedInstall', '-Vst3Dir', "`"$target`"")
            # Declining the prompt throws, and with ErrorActionPreference=Stop that used to end the
            # script here: no standalone, no browser edition, and a raw stack trace where the honest
            # answer is "you said no, so nothing was installed".
            try {
                $child = Start-Process -FilePath $ps -Verb RunAs -Wait -WindowStyle Hidden -PassThru -ArgumentList $quoted
                if (Test-Path -LiteralPath $LogPath) {
                    Get-Content -LiteralPath $LogPath | ForEach-Object { Write-Host "   elevated: $_" }
                }
                else { Write-Host "   elevated: process exit $($child.ExitCode), no log - the child never got as far as the install" -ForegroundColor Yellow }
            }
            catch {
                Say "no elevated install ($($_.Exception.Message)) - the standalone and the browser edition still start" 'Yellow'
            }
        }
    }
    else {
        try {
            $installed = Install-Vst3Bundle $Bundle $target
            Say "installed $dest ($($installed[$ModuleRel.ToLowerInvariant().Replace('\', '/')].Substring(0, 8))...)" 'Green'
            Remove-DuplicateBundles $target $installed
        }
        catch {
            # A DAW with the plugin loaded holds the module open; leaving the old install alone
            # beats terminating the host.
            Say "SKIPPED $dest - close the DAW and re-run ($($_.Exception.Message))" 'Yellow'
        }
    }

    if (-not $DryRun) {
        # Read the folder back instead of trusting the copy: the elevated child runs under a
        # different token in a console that has already closed, and an exit code of 0 from
        # Start-Process says nothing about what it wrote. Every file is compared, not just the
        # module, because a bundle missing its Resources installs green and fails to load.
        $landed = $false
        try {
            if (Test-Path -LiteralPath $dest) { $landed = Test-SameBundle $WantMap (Get-BundleHashMap $dest) }
        }
        catch { $landed = $false }
        if ($landed) { Say "check: $dest matches launch/ file for file ($($WantHash.Substring(0, 8))...)" 'Green' }
        else { Say "check: $dest is NOT the launch/ build - nothing was installed there" 'Red' }
        if ($InstallOnly) { if ($landed) { exit 0 } else { exit 1 } }
    }
}

# ---- 2. Standalone -----------------------------------------------------------------
Write-Step 'Standalone instrument'
if (Test-Path -LiteralPath $Exe) {
    if(-not (Skip "start $Exe")) { Start-Process -FilePath $Exe -WorkingDirectory $LaunchDir | Out-Null }
}
else {
    Write-Host "   Missing $Exe - run the launch export target first." -ForegroundColor Yellow
}

# ---- 3. Browser edition ------------------------------------------------------------
Write-Step 'Browser edition'
if (-not $IsRepoCheckout) {
    Say 'not part of this package - the browser edition is the src/ app, and runs from a repo checkout' 'Yellow'
    Say ("this folder has the VST3 bundle and $((Split-Path -Leaf $Exe)); install and standalone are done")
}
elseif (Test-Port 5173) {
    Write-Host "   dev server already listening on 5173"
    if (-not $DryRun) { Start-Process $Url | Out-Null }
}
elseif ($DryRun) {
    Write-Host '   [dry-run] npm run dev in a new window, wait for 5173, open the URL'
}
else {
    Start-Process -FilePath 'cmd.exe' -ArgumentList '/K', 'npm run dev' -WorkingDirectory $RepoRoot | Out-Null
    $ready = $false
    foreach ($i in 1..40) {
        Start-Sleep -Milliseconds 500
        if (Test-Port 5173) { $ready = $true; break }
    }
    # Opening the URL anyway would hand the browser a connection-refused page and read as "the
    # plugin is broken", so the tab only opens once something is proven to be listening.
    if ($ready) { Start-Process $Url | Out-Null }
    else { Write-Host '   dev server did not come up within 20s - not opening the URL' -ForegroundColor Yellow }
}

if ($IsRepoCheckout) {
    Write-Host 'Done. In Chrome or Edge, click a pad once to wake the audio engine.' -ForegroundColor Green
}
else {
    Write-Host 'Done. Rescan VST3 plugins in your DAW and load SliceForge - the standalone needs no rescan.' -ForegroundColor Green
}
