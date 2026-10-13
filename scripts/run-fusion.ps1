$ErrorActionPreference = "Stop"
$FusionRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Launcher = "$FusionRoot/build/pikmin1/bin/RelWithDebInfo/Pikmin 1 Launcher.exe"
if (-not (Test-Path $Launcher)) {
    $Launcher = "$FusionRoot/build/pikmin1/bin/Pikmin 1 Launcher.exe"
}
if (-not (Test-Path $Launcher)) {
    throw "Falta el launcher. Ejecuta primero scripts/build-fusion.ps1"
}

$env:NECTAR_PIKMIN1_DIR = "$FusionRoot/games/pikmin1"
$env:NECTAR_PIKMIN1_EXECUTABLE = "$FusionRoot/build/pikmin1/bin/RelWithDebInfo/Pikmin 1.exe"
if (-not (Test-Path $env:NECTAR_PIKMIN1_EXECUTABLE)) {
    $env:NECTAR_PIKMIN1_EXECUTABLE = "$FusionRoot/build/pikmin1/bin/Pikmin 1.exe"
}
$env:NECTAR_PIKMIN2_DIR = "$FusionRoot/games/pikmin2"
$env:NECTAR_PIKMIN2_EXECUTABLE = "$FusionRoot/build/pikmin2/RelWithDebInfo/pikmin2_pc.exe"
if (-not (Test-Path $env:NECTAR_PIKMIN2_EXECUTABLE)) {
    $env:NECTAR_PIKMIN2_EXECUTABLE = "$FusionRoot/build/pikmin2/pikmin2_pc.exe"
}

& $Launcher

