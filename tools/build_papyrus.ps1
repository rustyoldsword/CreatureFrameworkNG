# Compile papyrus\source\*.psc into papyrus\out\*.pex.
#
# Sources live in papyrus\, never scripts\: Windows is case-insensitive, so scripts\ would be the
# same folder as a deployed Scripts\. SexLab P+ and OSL Aroused sources do not parse with the
# Bethesda compiler, so papyrus\stubs stands in for them at build time (never shipped).
# A failed compile leaves the previous .pex in place, so every output is checked to be newer
# than its source.
#
# Run: powershell -ExecutionPolicy Bypass -File tools\build_papyrus.ps1 -Mods <MO2 mods dir> -Game <Skyrim dir>

param(
    [string]$Mods = $env:SKYRIM_MODS,   # MO2 mods folder containing the original CreatureFramework mod
    [string]$Game = $env:SKYRIM_GAME    # Skyrim SE folder with 'Papyrus Compiler' and Data\Scripts\Source
)

$ErrorActionPreference = 'Stop'
if (-not $Mods -or -not $Game) { throw 'Pass -Mods and -Game (or set SKYRIM_MODS / SKYRIM_GAME).' }

$root     = Split-Path -Parent $PSScriptRoot
$mods     = $Mods
$game     = $Game
$compiler = Join-Path $game 'Papyrus Compiler\PapyrusCompiler.exe'
$src      = Join-Path $root 'papyrus\source'
$out      = Join-Path $root 'papyrus\out'

New-Item -ItemType Directory -Force $out | Out-Null

$imports = @(
    $src
    (Join-Path $mods 'CreatureFramework\scripts\source')
    (Join-Path $root 'papyrus\stubs')
    (Join-Path $mods 'JContainers SE\scripts\source')
    (Join-Path $mods 'SkyUI\scripts\Source')
    (Join-Path $mods 'PapyrusUtil SE - Modders Scripting Utility Functions\Source\Scripts')
    (Join-Path $game 'Data\Scripts\Source')
    (Join-Path $game 'Data\Source\Scripts')
) -join ';'

$failed = @()
Push-Location $env:TEMP
try {
    foreach ($file in Get-ChildItem $src -Filter *.psc) {
        $output = & $compiler $file.FullName -f='TESV_Papyrus_Flags.flg' -i="$imports" -o="$out" 2>&1
        $pex = Join-Path $out ($file.BaseName + '.pex')
        if (-not (Test-Path $pex) -or (Get-Item $pex).LastWriteTime -lt $file.LastWriteTime) {
            $failed += $file.Name
            $output | Where-Object { $_ -match '\(\d+,\d+\)|error' } | ForEach-Object { Write-Host $_ }
        } else {
            Write-Host "ok  $($file.Name)"
        }
    }
} finally {
    Pop-Location
}

if ($failed.Count) {
    Write-Host "FAILED: $($failed -join ', ')"
    exit 1
}
