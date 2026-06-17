[CmdletBinding()]
param(
    [ValidateSet('ARM64', 'x64', 'Win32')][string]$Platform = 'ARM64',
    [ValidateSet('Debug', 'OptDebug', 'QA', 'Release')][string]$Configuration = 'Debug',
    [switch]$Build,
    [switch]$Deploy,
    [string]$AssetsDir = 'C:\projects\ASSETS-23.05.2026',
    [switch]$Wait
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

if ($Build) {
    & (Join-Path $PSScriptRoot 'build.ps1') -Platform $Platform -Configuration $Configuration
    if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)." }
}

$builtExe = Join-Path $repo "Apps\GameApp\_${Platform}${Configuration}\A51.exe"
$builtPdb = [IO.Path]::ChangeExtension($builtExe, '.pdb')
$launchExe = Join-Path $AssetsDir 'A51.exe'

if ($Deploy) {
    if (-not (Test-Path $builtExe)) { throw "Built exe not found: $builtExe (run with -Build first)." }
    Copy-Item $builtExe $launchExe -Force
    if (Test-Path $builtPdb) { Copy-Item $builtPdb ([IO.Path]::ChangeExtension($launchExe, '.pdb')) -Force }
    Write-Host "Deployed -> $launchExe"
}
if (-not (Test-Path $launchExe)) { throw "Launch exe not found: $launchExe (run with -Deploy)." }

$cdb = Get-ChildItem `
    "${env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\arm64\cdb.exe", `
    "${env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\x64\cdb.exe" `
    -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
if (-not $cdb) { throw 'Could not locate cdb.exe (install the Windows SDK Debugging Tools).' }

$logDir = Join-Path $repo 'tmp'
New-Item -ItemType Directory -Force $logDir | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$log = Join-Path $logDir "cdb_$stamp.log"
$dmp = Join-Path $logDir "crash_$stamp.dmp"
$cmdFile = Join-Path $logDir "cdb_cmds_$stamp.txt"

$handler = "kb 80; .echo === ALL THREADS ===; ~*kb; r; .dump /ma $dmp; .echo === CRASH DUMP: $dmp ===; .logclose; q"
@"
.logopen $log
.symfix
.sympath+ $AssetsDir
.reload
sxe -c "$handler" av
sxe -c "$handler" sbo
sxe -c "$handler" eh
sxe -c "$handler" ii
.echo === A51 RUNNING ($Platform|$Configuration) ===
g
"@ | Set-Content -Encoding ascii $cmdFile

Write-Host "cdb : $cdb"
Write-Host "exe : $launchExe"
Write-Host "log : $log"
Write-Host "dump: $dmp (only on crash)"

$cdbArgs = @('-g', '-G', '-cf', $cmdFile, $launchExe)
if ($Wait) {
    & $cdb @cdbArgs
}
else {
    Start-Process -FilePath $cdb -ArgumentList $cdbArgs -WorkingDirectory $AssetsDir
    Write-Host 'Launched (background). Tail the log with:'
    Write-Host "  Get-Content '$log' -Wait -Tail 40"
}
