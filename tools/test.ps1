param([string]$Zig = 'zig')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$out = Join-Path $root 'build/tests'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $root 'build/zig-global'
Push-Location $root
try {
  & $Zig cc -O2 -c tests/vendor/cJSON.c -o (Join-Path $out 'cJSON.o')
  if ($LASTEXITCODE -ne 0) { throw 'cJSON compilation failed.' }
  foreach ($name in @('sky','sky_local','sky_trails','gas','quake','fire','printer','space','deck','display')) {
    $exe = Join-Path $out "$name-test.exe"
    & $Zig c++ "tests/${name}_test.cpp" (Join-Path $out 'cJSON.o') -I tests/vendor -DWIDGET_DECK -std=c++17 -O2 -o $exe
    if ($LASTEXITCODE -ne 0) { throw "$name compilation failed." }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "$name test failed." }
  }
} finally { Pop-Location }
