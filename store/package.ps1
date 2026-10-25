# Makes the Xbox 360 Homebrew Store files for a release, in Build\:
#   UsbNet360-<version>.7z   the standard UsbNet360.xex, readme.txt and LICENSE.txt,
#                            which the store unpacks into Apps\UsbNet360\.
#   UsbNet360-<version>.ini  the store entry for that 7z (from consolemods.ini).
# -Version is the release's tag, such as 1.1.1. -Xex is the retail build to
# package: the one from Build from source in README.md, or the release's own.
# The experimental acksmall build stays on the Releases page only.
param([Parameter(Mandatory)][string]$Version, [string]$Xex = 'Build\CUsbEnet\UsbNet360.xex', [string]$SevenZip)
$ErrorActionPreference = 'Stop'
Push-Location (Split-Path $PSScriptRoot)
try {
    if ($Version -notmatch '^\d+\.\d+(\.\d+)?$') { throw "$Version isn't a version like 1.1.1." }
    if (!(Test-Path -LiteralPath $Xex)) { throw "$Xex is missing. Build it, or pass -Xex with the release's UsbNet360.xex." }
    if (!$SevenZip) { $SevenZip = (Get-Command 7z -ErrorAction SilentlyContinue).Source }
    if (!$SevenZip) { $SevenZip = Join-Path $env:ProgramFiles '7-Zip\7z.exe' }
    if (!(Test-Path -LiteralPath $SevenZip)) { throw '7-Zip is needed. Install it, or pass -SevenZip with the path to 7z.exe.' }
    $name = "UsbNet360-$Version"
    New-Item -ItemType Directory -Force Build | Out-Null
    $out = (Resolve-Path Build).Path
    # 7-Zip adds to an archive that exists, so the old files go first.
    foreach ($file in "$out\$name.7z", "$out\$name.ini") { if (Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file } }
    $stage = "Build\store\$name"
    New-Item -ItemType Directory -Force $stage | Out-Null
    Copy-Item -LiteralPath $Xex -Destination "$stage\UsbNet360.xex" -Force
    Copy-Item -LiteralPath LICENSE -Destination "$stage\LICENSE.txt" -Force
    (Get-Content -Raw store\readme.txt).Replace('@VERSION@', $Version) | Set-Content -NoNewline -Encoding ascii "$stage\readme.txt"
    # The files go at the top of the 7z, with no folder around them: the
    # store unpacks it into the entry's path itself. Default 7-Zip settings
    # (LZMA2), as the 7z files on the ConsoleMods repo use.
    Push-Location $stage
    try { & $SevenZip a -t7z -bso0 "$out\$name.7z" * } finally { Pop-Location }
    if ($LASTEXITCODE) { throw '7-Zip failed on the 7z.' }
    $size = (Get-Item -LiteralPath "$out\$name.7z").Length
    $sizeText = if ($size -ge 1MB) { ($size / 1MB).ToString('0.#', [Globalization.CultureInfo]::InvariantCulture) + 'MB' } else { [string][math]::Ceiling($size / 1KB) + 'KB' }
    (Get-Content -Raw store\consolemods.ini).Replace('@VERSION@', $Version).Replace('@SIZE@', $sizeText) | Set-Content -NoNewline -Encoding ascii "$out\$name.ini"
    Write-Host "Made Build\$name.7z ($sizeText) and Build\$name.ini."
    Write-Host "Attach the 7z to the GitHub release $Version. The ini's dataurl points at it there."
} finally { Pop-Location }
