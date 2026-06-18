[CmdletBinding()]
param(
    [ValidateSet('ARM64', 'x64', 'Win32')][string]$Platform = 'ARM64',
    [ValidateSet('Debug', 'OptDebug', 'QA', 'Release')][string]$Configuration = 'Debug',
    [switch]$PointerAudit,
    [switch]$Clean,
    [switch]$Rebuild
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

if (-not $env:VCPKG_ROOT) {
    if (Test-Path 'C:\vcpkg\vcpkg.exe') { $env:VCPKG_ROOT = 'C:\vcpkg' }
    else { throw 'VCPKG_ROOT is not set and C:\vcpkg was not found. Point it at a vcpkg checkout.' }
}

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
if (-not $vs) { throw 'Could not locate Visual Studio via vswhere.' }
$msbuild = Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'

$target = if ($Rebuild) { 'Rebuild' } elseif ($Clean) { 'Clean' } else { 'Build' }
$msbuildArgs = @(
    (Join-Path $repo 'Apps\GameApp\A51.vcxproj')
    "/t:$target"
    "/p:Configuration=$Configuration"
    "/p:Platform=$Platform"
    '/m'
    '/p:UseMultiToolTask=true'
    '/p:EnforceProcessCountAcrossBuilds=true'
    '/nologo'
)
if ($PointerAudit) { $msbuildArgs += '/p:A51PointerAudit=true' }

Write-Host "Building A51 $Configuration|$Platform (vcpkg: $env:VCPKG_ROOT)$(if ($PointerAudit) { ' [pointer audit]' })"
& $msbuild @msbuildArgs
exit $LASTEXITCODE
