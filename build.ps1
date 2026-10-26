# Builds the release files: NetUsb360.xex and NetUsb360-acksmall.xex, both for
# retail kernel 17559, plus the sample NetUsb360.ini, into Build\Release.
#
# Run from a Developer PowerShell (or any shell where msbuild is on PATH) on a
# PC with the Xbox 360 SDK installed:
#
#   .\build.ps1
#   .\build.ps1 -Variant acksmall
#   .\build.ps1 -Deploy        # also copy each XEX to the default console
param(
    [ValidateSet("all", "standard", "acksmall")]
    [string]$Variant = "all",
    [switch]$Deploy
)

$ErrorActionPreference = "Stop"
$Root = $PSScriptRoot
$Solution = Join-Path $Root "CUsbEnet.sln"
$OutputDir = Join-Path $Root "Build\Release"

# Each entry is one release file. The defines are the build-switch defaults
# from CUsbEnet\Main.h that the file is built with; NetUsb360.ini can still
# change the transmit options when the driver loads.
$Builds = [ordered]@{
    "standard" = @{ Xex = "NetUsb360.xex"; Defines = @("DEVKIT_ONLY=0") }
    "acksmall" = @{ Xex = "NetUsb360-acksmall.xex"; Defines = @(
        "DEVKIT_ONLY=0",
        "USBENET_AX88179_TX_AGGREGATION=1",
        "USBENET_COALESCE_TITLE_SMALL_FRAMES=1",
        "USBENET_TX_AGGREGATE_SMALL_ONLY=1") }
}

if (-not (Get-Command msbuild -ErrorAction SilentlyContinue)) {
    throw "msbuild was not found. Run this from a Developer PowerShell for Visual Studio."
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

foreach ($Name in $Builds.Keys) {
    if ($Variant -ne "all" -and $Variant -ne $Name) {
        continue
    }

    $Build = $Builds[$Name]
    # MSBuild reads ; as a property separator, so pass it escaped.
    $Defines = ($Build.Defines -join "%3B")
    $DeploymentType = if ($Deploy) { "CopyToHardDrive" } else { "None" }

    Write-Host "Building $($Build.Xex) ($Name): $($Build.Defines -join ', ')"
    & msbuild $Solution /t:Rebuild /m "/p:Configuration=Release" "/p:Platform=Xbox 360" `
        "/p:NetUsb360Defines=$Defines" "/p:NetUsb360XexName=$($Build.Xex)" "/p:DeploymentType=$DeploymentType"
    if ($LASTEXITCODE -ne 0) {
        throw "Build of $($Build.Xex) failed."
    }

    Copy-Item (Join-Path $Root "Build\CUsbEnet\$($Build.Xex)") $OutputDir -Force
}

Copy-Item (Join-Path $Root "release\NetUsb360.ini") $OutputDir -Force
Write-Host "Release files are in $OutputDir"
