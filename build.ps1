<#
.SYNOPSIS
    Baut Sample Instrument Studio unter Windows.

.DESCRIPTION
    Sucht Visual Studio (inkl. Insiders/Preview) über vswhere, richtet die
    Compiler-Umgebung ein und ruft CMake mit Ninja auf. Damit sind weder
    "Developer PowerShell" noch PATH-Einträge nötig.

.EXAMPLE
    .\build.ps1                      # Debug bauen
    .\build.ps1 -Run                 # Debug bauen und die App starten
    .\build.ps1 -Config Release      # Release bauen (alle Formate)
    .\build.ps1 -Target SisClipGeometryTests -Run
#>
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Config = 'Debug',
    [string]$Target = 'SampleInstrumentStudio_Standalone',
    [switch]$Run,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$buildDir = Join-Path $root "build\ninja-$($Config.ToLower())"

# --- Visual Studio finden ---
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "vswhere nicht gefunden - bitte Visual Studio oder die Build Tools installieren." }

$vsPath = & $vswhere -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) {
    # Ältere vswhere-Versionen kennen neue Produkte nicht: Ordner direkt prüfen
    $vsPath = Get-ChildItem "$env:ProgramFiles\Microsoft Visual Studio", "${env:ProgramFiles(x86)}\Microsoft Visual Studio" -Directory -ErrorAction SilentlyContinue |
        ForEach-Object { Get-ChildItem $_.FullName -Directory -ErrorAction SilentlyContinue } |
        Where-Object { Test-Path (Join-Path $_.FullName 'VC\Auxiliary\Build\vcvars64.bat') } |
        Select-Object -Last 1 -ExpandProperty FullName
}
if (-not $vsPath) { throw "Kein C++-Toolset gefunden (Workload 'Desktopentwicklung mit C++')." }

$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $vcvars)) { throw "vcvars64.bat fehlt unter $vsPath" }

# --- CMake und Ninja finden ---
$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) {
    $cmake = @("$env:ProgramFiles\CMake\bin\cmake.exe",
               (Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe')) |
        Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $cmake) { throw "cmake.exe nicht gefunden." }

$ninjaDir = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja'
$extraPath = @((Split-Path $cmake), $ninjaDir) -join ';'

if ($Clean -and (Test-Path $buildDir)) { Remove-Item -LiteralPath $buildDir -Recurse -Force }

Write-Host "Visual Studio: $vsPath"
Write-Host "CMake:         $cmake"
Write-Host "Build-Ordner:  $buildDir`n"

# vcvars setzt den PATH; eigene Einträge davor, damit sie erhalten bleiben
$configure = "set `"PATH=$extraPath;%PATH%`" && `"$vcvars`" >nul && cmake -G Ninja -S `"$root`" -B `"$buildDir`" -DCMAKE_BUILD_TYPE=$Config"
$build     = "set `"PATH=$extraPath;%PATH%`" && `"$vcvars`" >nul && cmake --build `"$buildDir`" --target $Target"

# vcvars64.bat schreibt harmlose Warnungen nach stderr (z. B. ein fehlendes vswhere.exe).
# Mit 'Stop' würde PowerShell das als Abbruch werten - hier zählt allein der Rückgabewert.
$ErrorActionPreference = 'Continue'

cmd /c $configure 2>&1 | Write-Host
if ($LASTEXITCODE -ne 0) { throw "CMake-Konfiguration fehlgeschlagen." }
cmd /c $build 2>&1 | Write-Host
if ($LASTEXITCODE -ne 0) { throw "Build fehlgeschlagen." }

$ErrorActionPreference = 'Stop'

if ($Run) {
    $exe = Join-Path $buildDir "SampleInstrumentStudio_artefacts\$Config\Standalone\Sample Instrument Studio.exe"
    if ($Target -eq 'SisClipGeometryTests') { $exe = Join-Path $buildDir 'SisClipGeometryTests.exe' }
    if (-not (Test-Path $exe)) { throw "Nichts zum Starten gefunden: $exe" }
    Write-Host "`nStarte $exe"
    & $exe
}
