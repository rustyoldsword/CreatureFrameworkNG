# Assemble the standalone release: dist\Creature Framework NG\ (Data layout) and a .7z next to it.
# Run after: xmake build CreatureFrameworkNG, tools\build_papyrus.ps1, tools\rebuild_esm.py
#
# Run: powershell -ExecutionPolicy Bypass -File tools\package.ps1 -OriginalMod <original CreatureFramework mod dir>

param([string]$OriginalMod = $env:CF_ORIGINAL_MOD)

$ErrorActionPreference = 'Stop'
if (-not $OriginalMod) { throw 'Pass -OriginalMod (or set CF_ORIGINAL_MOD).' }

$root    = Split-Path -Parent $PSScriptRoot
$cf      = $OriginalMod
$version = '4.1.1'
$stage   = Join-Path $root 'dist\Creature Framework NG'
$archive = Join-Path $root "dist\Creature Framework NG $version.7z"

if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force "$stage\SKSE\Plugins", "$stage\Scripts\Source", "$stage\Interface\translations", "$stage\SEQ" | Out-Null

Copy-Item (Join-Path $root 'plugin\CreatureFramework.esm') $stage
Copy-Item (Join-Path $cf 'SEQ\CreatureFramework.seq') "$stage\SEQ\"
Copy-Item (Join-Path $root 'build\windows\x64\releasedbg\CreatureFrameworkNG.dll') "$stage\SKSE\Plugins\"
Copy-Item (Join-Path $root 'dist_template\SKSE\Plugins\CreatureFrameworkNG.ini') "$stage\SKSE\Plugins\"
Copy-Item (Join-Path $root 'dist_template\README - Creature Framework NG.txt') $stage
Copy-Item (Join-Path $root 'dist_template\README - Creature Framework NG (RU).txt') $stage
Copy-Item (Join-Path $root 'translations\*.txt') "$stage\Interface\translations\"

$scripts = 'CreatureFramework', 'CreatureFrameworkNative', 'CreatureFrameworkConfig', 'CreatureFrameworkUtility', 'CreatureFrameworkPuppet'
foreach ($s in $scripts) {
    $pex = Join-Path $root "papyrus\out\$s.pex"
    $psc = Join-Path $root "papyrus\source\$s.psc"
    if ((Get-Item $pex).LastWriteTime -lt (Get-Item $psc).LastWriteTime) { throw "$s.pex is older than its source - run tools\build_papyrus.ps1" }
    Copy-Item $pex "$stage\Scripts\"
    Copy-Item $psc "$stage\Scripts\Source\"
}

# verification: plugin form versions, expected file set
$esm = [IO.File]::ReadAllBytes("$stage\CreatureFramework.esm")
$off = 0; $bad = 0
while ($off + 24 -le $esm.Length) {
    $type = [Text.Encoding]::ASCII.GetString($esm, $off, 4)
    $size = [BitConverter]::ToUInt32($esm, $off + 4)
    if ($type -eq 'GRUP') { $off += 24; continue }
    if ([BitConverter]::ToUInt16($esm, $off + 20) -ne 44) { $bad++ }
    $off += 24 + $size
}
if ($bad) { throw "$bad record(s) in CreatureFramework.esm are not form 44" }

$expected = 1 + 1 + 2 + 2 + 5 + 5 + 10
$files = Get-ChildItem $stage -Recurse -File
if ($files.Count -ne $expected) { throw "expected $expected files, staged $($files.Count)" }

$files | Select-Object @{ n = 'file'; e = { $_.FullName.Substring($stage.Length) } }, Length | Format-Table -AutoSize | Out-String -Width 200

$sevenZip = @('C:\Program Files\7-Zip\7z.exe', 'C:\Program Files (x86)\7-Zip\7z.exe') | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $sevenZip) { Write-Host "7-Zip not found - staged folder only: $stage"; return }
if (Test-Path $archive) { Remove-Item $archive -Force }
Push-Location $stage
try { & $sevenZip a -t7z -mx=9 $archive * | Select-Object -Last 2 } finally { Pop-Location }
Get-Item $archive | Select-Object FullName, Length
