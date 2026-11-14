<#
    SKEER SliceForge launcher.

    1. Refreshes the installed SliceForge.vst3 bundle from launch\ so a new build reaches the DAW.
    2. Starts the standalone instrument.
    3. Serves the browser edition (reuses a dev server that is already listening) and opens it.

        launch\start.bat                 all three
        launch\start.ps1 -DryRun         show the plan, change nothing
        launch\start.ps1 -Vst3Dir "..."  install into a specific VST3 folder
#>
[CmdletBinding()]
param(
    [switch]$DryRun,
    [string]$Vst3Dir = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$LaunchDir = $PSScriptRoot
$RepoRoot = Split-Path -Parent $LaunchDir
$Exe = Join-Path $LaunchDir 'SliceForge.exe'
$Bundle = Join-Path $LaunchDir 'SliceForge.vst3'
$Url = 'http://127.0.0.1:5173/'

function Write-Step([string]$Message) { Write-Host ('>> ' + $Message) -ForegroundColor Cyan }
function Skip([string]$Message) { if ($DryRun) { Write-Host ('   [dry-run] ' + $Message); return $true } return $false }

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
$BundleDll = Join-Path $Bundle 'Contents\x86_64-win\SliceForge.vst3'
if (-not (Test-Path -LiteralPath $BundleDll)) {
    Write-Host "   Missing $BundleDll - run: cmake --build native/build --config Release --target launch" -ForegroundColor Yellow
}
else {
    if ($Vst3Dir) {
        $targets = @($Vst3Dir)
    }
    else {
        # Update whichever folder already carries a SliceForge install rather than guessing the host's
        # custom scan path; candidates are the three this project has been installed into.
        $candidates = @(
            (Join-Path $env:USERPROFILE 'Documents\VST3'),
            (Join-Path $env:LOCALAPPDATA 'Programs\Common\VST3'),
            (Join-Path $env:ProgramFiles 'Common Files\VST3')
        )
        $targets = @($candidates | Where-Object { Test-Path -LiteralPath (Join-Path $_ 'SliceForge.vst3') })
    }

    if ($targets.Count -eq 0) {
        # First run under the new name: nothing matches yet, so install where the other SKEER
        # plugins live and where this machine's DAW actually scans.
        $fallback = Join-Path $env:USERPROFILE 'Documents\VST3'
        Write-Host "   no existing SliceForge.vst3 install found - installing into $fallback"
        $targets = @($fallback)
    }
    foreach ($dir in $targets) {
        $dest = Join-Path $dir 'SliceForge.vst3'
        if (Skip "copy $Bundle -> $dest") { continue }
        try {
            New-Item -ItemType Directory -Force $dir | Out-Null
            if (Test-Path -LiteralPath $dest) { Remove-Item -LiteralPath $dest -Recurse -Force }
            Copy-Item -LiteralPath $Bundle -Destination $dest -Recurse -Force
            Write-Host "   updated $dest"
        }
        catch {
            # A DAW with the plugin loaded holds the module open; skipping is safer than killing it.
            Write-Host "   SKIPPED $dest - close the DAW and re-run ($($_.Exception.Message))" -ForegroundColor Yellow
        }
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
if (Test-Port 5173) {
    Write-Host "   dev server already listening on 5173"
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
    if (-not $ready) { Write-Host '   dev server did not come up within 20s' -ForegroundColor Yellow }
}
if (-not $DryRun) { Start-Process $Url | Out-Null }

Write-Host 'Done. In Chrome or Edge, click a pad once to wake the audio engine.' -ForegroundColor Green
