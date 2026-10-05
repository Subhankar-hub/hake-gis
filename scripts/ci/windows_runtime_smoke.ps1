#Requires -Version 5.1
<#
.SYNOPSIS
  Package assertions, NSIS install and clean-environment runtime validation for
  Hake GeoDesk Windows CI.

.DESCRIPTION
  Every runtime check runs from a sanitized environment: PATH is reduced to the
  Windows system directories, PYTHON*/PROJ_*/GDAL_*/QT_* variables are removed,
  and a decoy directory (fake GDAL DLLs and tools, an invalid proj.db, a broken
  osgeo package, an empty PYTHONHOME) is put in front so that any accidental use
  of a non-bundled dependency fails loudly.

  Scenarios:
    isolated-env   bundled python.exe, nothing but sanitized PATH (+ decoy)
    launcher-env   bundled python.exe with the expanded hake-geodesk.env applied
    process        hake-geodesk-process.exe run gdal:polygonize with hostile
                   PROJ_*/GDAL_DATA/PYTHONHOME/PYTHONPATH and no launcher env
    gui            hake-geodesk.exe --code hake_gui_smoke.py (offscreen) with the
                   same hostile environment
    manifest       static PE dependency graph of the installed tree
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,

    [Parameter(Mandatory = $true)]
    [string]$RepoRoot,

    [Parameter(Mandatory = $false)]
    [string]$ReportDir = "",

    [Parameter(Mandatory = $false)]
    [ValidateSet("All", "AssertZip", "Install", "Smoke", "Process", "Gui", "Manifest", "Runtime", "UnicodeDiag")]
    [string]$Mode = "All",

    [Parameter(Mandatory = $false)]
    [string]$InstallTarget = "C:\Hake GeoDesk",

    [Parameter(Mandatory = $false)]
    [string]$ManifestPython = "python",

    [Parameter(Mandatory = $false)]
    [int]$GuiTimeoutSeconds = 600
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if (-not $ReportDir) {
    $ReportDir = Join-Path $BuildDir "runtime-smoke"
}
New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null
$ReportDir = (Resolve-Path -LiteralPath $ReportDir).Path

$DecoyRoot = "C:\hake-decoy"
$CleanUnset = @(
    "PYTHONHOME", "PYTHONPATH", "PYTHONSTARTUP", "PYTHONUSERBASE", "PYTHONNOUSERSITE",
    "PROJ_DATA", "PROJ_LIB", "GDAL_DATA", "GDAL_DRIVER_PATH", "QT_PLUGIN_PATH",
    "QGIS_PREFIX_PATH", "CONDA_PREFIX", "VIRTUAL_ENV", "OSGEO4W_ROOT", "QT_QPA_PLATFORM"
)

function Write-Section([string]$Title) {
    Write-Host ""
    Write-Host "==== $Title ===="
}

# ---------------------------------------------------------------------------
# Package assertions
# ---------------------------------------------------------------------------

function Get-CPackZip {
    $zips = Get-ChildItem -Path $BuildDir -Filter *.zip -ErrorAction SilentlyContinue |
        Sort-Object FullName
    if (-not $zips) {
        throw "No CPack zip in $BuildDir"
    }
    return $zips[0]
}

function Assert-ZipContents {
    Write-Section "Package content assertion (ZIP)"
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = Get-CPackZip
    Write-Host "Inspecting $($zip.FullName)"
    $archive = [System.IO.Compression.ZipFile]::OpenRead($zip.FullName)
    try {
        $names = @($archive.Entries | ForEach-Object { ($_.FullName -replace '\\', '/') })

        $required = [ordered]@{
            "share/proj/proj.db"                   = '(^|/)share/proj/proj\.db$'
            "share/gdal/*"                         = '(^|/)share/gdal/'
            "bin/python.exe"                       = '(^|/)bin/python\.exe$'
            "bin/gdal_polygonize.bat"              = '(^|/)bin/gdal_polygonize\.bat$'
            "bin/Scripts/gdal_polygonize.py"       = '(^|/)bin/Scripts/gdal_polygonize\.py$'
            "bin/Lib/site-packages/osgeo/"         = '(^|/)bin/Lib/site-packages/osgeo/'
            "bin/hake-geodesk.env"                 = '(^|/)bin/hake-geodesk\.env$'
            "bin/qt.conf"                          = '(^|/)bin/qt\.conf$'
            "bin/hake-geodesk-process.exe"         = '(^|/)bin/hake-geodesk-process\.exe$'
            "share/doc/hake-geodesk/third-party/*" = '(^|/)share/doc/hake-geodesk/third-party/[^/]+/copyright$'
        }
        $missing = @()
        foreach ($key in $required.Keys) {
            $hits = @($names | Where-Object { $_ -match $required[$key] })
            if (-not $hits) { $missing += $key }
        }
        $licenses = @($names | Where-Object { $_ -match $required["share/doc/hake-geodesk/third-party/*"] })
        Write-Host "Third-party license notices packaged: $($licenses.Count)"
        if ($missing.Count -gt 0) {
            throw "ZIP missing required packaged paths: $($missing -join ', ')"
        }

        $envEntry = $archive.Entries | Where-Object {
            ($_.FullName -replace '\\', '/') -match '(^|/)bin/hake-geodesk\.env$'
        } | Select-Object -First 1
        $reader = New-Object System.IO.StreamReader($envEntry.Open())
        try { $envText = $reader.ReadToEnd() } finally { $reader.Dispose() }
        Write-Host "Packaged hake-geodesk.env:"
        Write-Host $envText
        foreach ($req in @(
            'PROJ_DATA={prefix}\share\proj',
            'PROJ_LIB={prefix}\share\proj',
            'GDAL_DATA={prefix}\share\gdal'
        )) {
            if ($envText -notlike "*$req*") {
                throw "Packaged hake-geodesk.env missing required line: $req"
            }
        }
        Write-Host "ZIP content assertion PASSED"
    }
    finally {
        $archive.Dispose()
    }
}

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------

function Install-NsisArtifact([string]$Target) {
    Write-Section "Silent NSIS install"
    $exes = @(Get-ChildItem -Path $BuildDir -Filter "*.exe" -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like "Hake-Geodesk-Installer*" -or $_.Name -like "*Installer*.exe" } |
        Sort-Object FullName)
    if (-not $exes) {
        $exes = @(Get-ChildItem -Path $BuildDir -Filter "*.exe" -Recurse -ErrorAction SilentlyContinue |
            Where-Object { $_.Length -gt 10MB } |
            Sort-Object Length -Descending)
    }
    if (-not $exes) {
        throw "No NSIS installer .exe found in $BuildDir"
    }
    $installer = $exes[0]
    # ASCII install folder (HAKE_PRODUCT_INSTALL_DIRECTORY) with a space in it.
    Write-Host "Installing $($installer.FullName) silently into $Target ..."
    # NSIS: /S = silent, /D= must be last and unquoted
    $proc = Start-Process -FilePath $installer.FullName -ArgumentList "/S", "/D=$Target" -Wait -PassThru
    if ($proc.ExitCode -ne 0) {
        throw "NSIS installer exited with code $($proc.ExitCode)"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $Target "bin\hake-geodesk.exe"))) {
        throw "NSIS install finished but bin\hake-geodesk.exe missing under $Target"
    }
    Write-Host "NSIS installer completed"
}

function Find-InstallRoot {
    $saved = Join-Path $ReportDir "install_root.txt"
    if (Test-Path -LiteralPath $saved) {
        return (Get-Content -LiteralPath $saved -Raw -Encoding UTF8).Trim()
    }
    if (Test-Path -LiteralPath (Join-Path $InstallTarget "bin\hake-geodesk.exe")) {
        return (Resolve-Path -LiteralPath $InstallTarget).Path
    }
    throw "Could not locate installed Hake GeoDesk under $InstallTarget"
}

function Assert-InstalledLayout([string]$InstallRoot) {
    Write-Section "Installed layout"
    foreach ($rel in @("share\proj\proj.db", "bin\qt.conf", "bin\python.exe", "bin\Lib\os.py",
                       "bin\hake-geodesk-process.exe", "bin\gdal_polygonize.bat")) {
        $p = Join-Path $InstallRoot $rel
        if (-not (Test-Path -LiteralPath $p)) {
            throw "Installed tree missing $rel"
        }
        Write-Host "  OK $rel"
    }
}

# ---------------------------------------------------------------------------
# Environment control
# ---------------------------------------------------------------------------

function New-DecoyRoot {
    Write-Section "Create decoy dependencies at $DecoyRoot"
    if (Test-Path -LiteralPath $DecoyRoot) { Remove-Item -Recurse -Force -LiteralPath $DecoyRoot }
    foreach ($d in @("bin", "proj", "gdal", "pyhome", "python\osgeo", "python\qgis")) {
        New-Item -ItemType Directory -Force -Path (Join-Path $DecoyRoot $d) | Out-Null
    }
    $junk = [byte[]](1..64)
    foreach ($dll in @("gdal.dll", "proj_9.dll", "proj.dll", "geos_c.dll", "sqlite3.dll",
                       "Qt6Core.dll", "python312.dll", "qgis_core.dll")) {
        [System.IO.File]::WriteAllBytes((Join-Path $DecoyRoot "bin\$dll"), $junk)
    }
    foreach ($tool in @("gdal_polygonize", "gdalinfo", "ogr2ogr", "gdal_translate", "gdalwarp", "python")) {
        Set-Content -LiteralPath (Join-Path $DecoyRoot "bin\$tool.bat") -Encoding ASCII -Value @(
            "@echo off",
            "echo DECOY $tool from PATH was executed 1>&2",
            "exit /b 97"
        )
    }
    Set-Content -LiteralPath (Join-Path $DecoyRoot "proj\proj.db") -Encoding ASCII -Value "decoy - not a PROJ database"
    foreach ($pkg in @("osgeo", "qgis")) {
        Set-Content -LiteralPath (Join-Path $DecoyRoot "python\$pkg\__init__.py") -Encoding ASCII `
            -Value "raise ImportError('DECOY $pkg imported from PYTHONPATH')"
    }
}

function Get-SanitizedPath([switch]$WithDecoy) {
    $windir = $env:WINDIR
    $parts = @("$windir\system32", $windir, "$windir\System32\Wbem", "$windir\System32\WindowsPowerShell\v1.0")
    if ($WithDecoy) { $parts = @("$DecoyRoot\bin") + $parts }
    return ($parts -join ";")
}

function Invoke-WithEnvironment([hashtable]$Set, [string[]]$Unset, [scriptblock]$Body) {
    $keys = @($Set.Keys) + $Unset | Select-Object -Unique
    $saved = @{}
    foreach ($k in $keys) { $saved[$k] = [Environment]::GetEnvironmentVariable($k, "Process") }
    try {
        foreach ($k in $Unset) { [Environment]::SetEnvironmentVariable($k, $null, "Process") }
        foreach ($k in $Set.Keys) { [Environment]::SetEnvironmentVariable($k, [string]$Set[$k], "Process") }
        & $Body
    }
    finally {
        foreach ($k in $keys) { [Environment]::SetEnvironmentVariable($k, $saved[$k], "Process") }
    }
}

function Get-IsolatedEnv {
    return @{ PATH = (Get-SanitizedPath -WithDecoy) }
}

function Get-HostileEnv {
    return @{
        PATH            = (Get-SanitizedPath -WithDecoy)
        PROJ_DATA       = "$DecoyRoot\proj"
        PROJ_LIB        = "$DecoyRoot\proj"
        GDAL_DATA       = "$DecoyRoot\gdal"
        PYTHONHOME      = "$DecoyRoot\pyhome"
        PYTHONPATH      = "$DecoyRoot\python"
        QT_QPA_PLATFORM = "offscreen"
    }
}

function Get-LauncherEnv([string]$InstallRoot) {
    $appDir = Join-Path $InstallRoot "bin"
    $envFile = Join-Path $appDir "hake-geodesk.env"
    $map = @{ PATH = (Get-SanitizedPath -WithDecoy) }
    Get-Content -LiteralPath $envFile -Encoding UTF8 | ForEach-Object {
        $line = $_.TrimEnd("`r")
        if (-not $line -or $line.StartsWith("#")) { return }
        $eq = $line.IndexOf("=")
        if ($eq -lt 1) { return }
        $name = $line.Substring(0, $eq)
        $value = $line.Substring($eq + 1).Replace("{app}", $appDir).Replace("{prefix}", $InstallRoot)
        if ($name -ieq "PATH") { $value = "$value;$($map.PATH)" }
        $map[$name] = $value
    }
    return $map
}

function Copy-SmokeScripts {
    $dest = Join-Path $ReportDir "scripts"
    New-Item -ItemType Directory -Force -Path $dest | Out-Null
    foreach ($f in @("hake_runtime_smoke.py", "windows_runtime_smoke.py", "hake_gui_smoke.py",
                     "hake_runtime_deps.json", "hake_dependency_manifest.py")) {
        Copy-Item -LiteralPath (Join-Path $RepoRoot "scripts\ci\$f") -Destination $dest -Force
    }
    return $dest
}

function Invoke-BundledPython([string]$InstallRoot, [string[]]$PyArgs) {
    $python = Join-Path $InstallRoot "bin\python.exe"
    Invoke-WithEnvironment (Get-IsolatedEnv) $CleanUnset {
        & $python @PyArgs
        if ($LASTEXITCODE -ne 0) { throw "bundled python.exe $($PyArgs -join ' ') exited $LASTEXITCODE" }
    }
}

# ---------------------------------------------------------------------------
# Scenarios
# ---------------------------------------------------------------------------

function Invoke-PythonSmoke([string]$InstallRoot, [string]$Label, [hashtable]$EnvSet) {
    Write-Section "Runtime smoke ($Label)"
    $scripts = Copy-SmokeScripts
    $python = Join-Path $InstallRoot "bin\python.exe"
    Invoke-WithEnvironment $EnvSet $CleanUnset {
        Write-Host "[$Label] PATH=$env:PATH"
        Write-Host "[$Label] PROJ_LIB=$env:PROJ_LIB PYTHONHOME=$env:PYTHONHOME"
        & $python -u (Join-Path $scripts "hake_runtime_smoke.py") `
            --platform windows `
            --install-root $InstallRoot `
            --report-dir $ReportDir `
            --label $Label `
            --forbid $DecoyRoot
        if ($LASTEXITCODE -ne 0) { throw "Smoke test '$Label' failed with exit $LASTEXITCODE" }
    }
}

function Invoke-ProcessPolygonize([string]$InstallRoot) {
    Write-Section "hake-geodesk-process (no launcher env, hostile environment)"
    $processExe = Join-Path $InstallRoot "bin\hake-geodesk-process.exe"
    $work = Join-Path $ReportDir "process-work"
    New-Item -ItemType Directory -Force -Path $work | Out-Null
    $tif = Join-Path $work "process_input.tif"
    $out = Join-Path $work "process_output.gpkg"
    if (Test-Path -LiteralPath $out) { Remove-Item -LiteralPath $out -Force }

    $mk = Join-Path $work "mk_raster.py"
    Set-Content -LiteralPath $mk -Encoding UTF8 -Value @"
import sys
from osgeo import gdal, osr
gdal.UseExceptions()
ds = gdal.GetDriverByName('GTiff').Create(sys.argv[1], 32, 32, 1, gdal.GDT_Byte)
srs = osr.SpatialReference(); srs.ImportFromEPSG(2193)
ds.SetProjection(srs.ExportToWkt())
ds.SetGeoTransform([1600000.0, 10.0, 0.0, 5500000.0, 0.0, -10.0])
ds.GetRasterBand(1).WriteRaster(0, 0, 32, 32, bytes([1] * 16 + [2] * 16) * 32)
ds = None
"@
    Invoke-BundledPython $InstallRoot @("-u", $mk, $tif)

    $log = Join-Path $ReportDir "process_polygonize.log"
    Invoke-WithEnvironment (Get-HostileEnv) $CleanUnset {
        Write-Host "PATH=$env:PATH"
        Write-Host "PROJ_LIB=$env:PROJ_LIB GDAL_DATA=$env:GDAL_DATA PYTHONHOME=$env:PYTHONHOME PYTHONPATH=$env:PYTHONPATH"
        & $processExe --version *> (Join-Path $ReportDir "process_version.log")
        $code = $LASTEXITCODE
        Get-Content -LiteralPath (Join-Path $ReportDir "process_version.log") | Write-Host
        if ($code -ne 0) { throw "hake-geodesk-process --version exited $code" }

        $procArgs = @("run", "gdal:polygonize", "--", "INPUT=$tif", "BAND=1", "FIELD=DN",
                      "EIGHT_CONNECTEDNESS=false", "OUTPUT=$out")
        Write-Host "Running: $processExe $($procArgs -join ' ')"
        & $processExe @procArgs *> $log
        $code = $LASTEXITCODE
        Get-Content -LiteralPath $log -ErrorAction SilentlyContinue | Write-Host
        if ($code -ne 0) { throw "hake-geodesk-process exited $code (see process_polygonize.log)" }
    }
    if (Select-String -LiteralPath $log -Pattern "DECOY" -SimpleMatch -Quiet) {
        throw "hake-geodesk-process executed a decoy tool from PATH"
    }
    if (-not (Test-Path -LiteralPath $out)) {
        throw "process OUTPUT GeoPackage missing: $out"
    }

    $validate = Join-Path $work "validate_out.py"
    Set-Content -LiteralPath $validate -Encoding UTF8 -Value @"
import sys
from osgeo import ogr
ogr.UseExceptions()
ds = ogr.Open(sys.argv[1])
layer = ds.GetLayer(0)
count = layer.GetFeatureCount()
print('layer', layer.GetName(), 'features', count)
assert count >= 2, count
"@
    Invoke-BundledPython $InstallRoot @("-u", $validate, $out)
    Write-Host "hake-geodesk-process polygonize PASSED"
}

function Invoke-GuiSmoke([string]$InstallRoot) {
    Write-Section "GUI smoke (hake-geodesk.exe --code, offscreen, hostile environment)"
    $scripts = Copy-SmokeScripts
    $exe = Join-Path $InstallRoot "bin\hake-geodesk.exe"
    $profiles = Join-Path $ReportDir "gui-profiles"
    $report = Join-Path $ReportDir "gui-smoke.json"
    if (Test-Path -LiteralPath $report) { Remove-Item -LiteralPath $report -Force }
    New-Item -ItemType Directory -Force -Path $profiles | Out-Null

    $envSet = Get-HostileEnv
    $envSet["HAKE_GUI_SMOKE_REPORT"] = $report
    Invoke-WithEnvironment $envSet $CleanUnset {
        $guiArgs = @("--nologo", "--profiles-path", "`"$profiles`"", "--code", "`"$(Join-Path $scripts 'hake_gui_smoke.py')`"")
        $p = Start-Process -FilePath $exe -ArgumentList $guiArgs -PassThru
        $null = $p.Handle  # keep the process handle so ExitCode is available after exit
        if (-not $p.WaitForExit($GuiTimeoutSeconds * 1000)) {
            Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
            throw "hake-geodesk.exe did not finish the GUI smoke within $GuiTimeoutSeconds s"
        }
        $script:GuiExit = $p.ExitCode
    }
    if (-not (Test-Path -LiteralPath $report)) {
        throw "GUI smoke wrote no report (exit $script:GuiExit)"
    }
    $result = Get-Content -LiteralPath $report -Raw -Encoding UTF8 | ConvertFrom-Json
    Get-Content -LiteralPath $report -Raw -Encoding UTF8 | Write-Host
    if (-not $result.ok -or $script:GuiExit -ne 0) {
        throw "GUI smoke failed (exit $script:GuiExit): $($result.errors -join '; ')"
    }
    Write-Host "GUI smoke PASSED"
}

function Invoke-Manifest([string]$InstallRoot) {
    Write-Section "Runtime dependency manifest"
    $scripts = Copy-SmokeScripts
    $out = Join-Path $ReportDir "runtime-deps-windows.json"
    $manifestArgs = @((Join-Path $scripts "hake_dependency_manifest.py"),
                      "--platform", "windows", "--root", $InstallRoot, "--out", $out,
                      "--forbid", $DecoyRoot)
    foreach ($label in @("isolated-env", "launcher-env")) {
        $r = Join-Path $ReportDir "$label.json"
        if (Test-Path -LiteralPath $r) { $manifestArgs += @("--smoke-report", $r) }
    }
    $runner = Get-Command $ManifestPython -ErrorAction SilentlyContinue
    if ($runner) {
        & $runner.Source @manifestArgs
        if ($LASTEXITCODE -ne 0) { throw "Dependency manifest reported errors (see $out)" }
    }
    else {
        Write-Host "$ManifestPython not found; running the manifest with the bundled interpreter"
        Invoke-BundledPython $InstallRoot $manifestArgs
    }
}

function Invoke-UnicodeDiagnostic([string]$InstallRoot) {
    Write-Section "Unicode install path diagnostic (relocated copy)"
    $unicodeRoot = "C:\Hake G$([char]0x00E9)oDesk $([char]0x2013) $([char]0x00DC)nicode"
    if (Test-Path -LiteralPath $unicodeRoot) { Remove-Item -Recurse -Force -LiteralPath $unicodeRoot }
    Copy-Item -Recurse -LiteralPath $InstallRoot -Destination $unicodeRoot
    Invoke-PythonSmoke -InstallRoot $unicodeRoot -Label "unicode-path" -EnvSet (Get-IsolatedEnv)
}

# ---- main ----
Write-Host "BuildDir=$BuildDir"
Write-Host "RepoRoot=$RepoRoot"
Write-Host "ReportDir=$ReportDir"
Write-Host "Mode=$Mode"

if ($Mode -in @("All", "AssertZip")) {
    Assert-ZipContents
}

$installRoot = $null
if ($Mode -in @("All", "Install", "Runtime")) {
    Install-NsisArtifact $InstallTarget
    $installRoot = (Resolve-Path -LiteralPath $InstallTarget).Path
    Set-Content -LiteralPath (Join-Path $ReportDir "install_root.txt") -Value $installRoot -Encoding UTF8
}
if ($Mode -ne "AssertZip" -and -not $installRoot) {
    $installRoot = Find-InstallRoot
}
if ($installRoot) {
    Assert-InstalledLayout $installRoot
    New-DecoyRoot
}

if ($Mode -in @("All", "Smoke", "Runtime")) {
    Invoke-PythonSmoke -InstallRoot $installRoot -Label "isolated-env" -EnvSet (Get-IsolatedEnv)
    Invoke-PythonSmoke -InstallRoot $installRoot -Label "launcher-env" -EnvSet (Get-LauncherEnv $installRoot)
}
if ($Mode -in @("All", "Process", "Runtime")) {
    Invoke-ProcessPolygonize -InstallRoot $installRoot
}
if ($Mode -in @("All", "Gui", "Runtime")) {
    Invoke-GuiSmoke -InstallRoot $installRoot
}
if ($Mode -in @("All", "Manifest", "Runtime")) {
    Invoke-Manifest -InstallRoot $installRoot
}
if ($Mode -eq "UnicodeDiag") {
    Invoke-UnicodeDiagnostic -InstallRoot $installRoot
}

Write-Section "All requested modes completed successfully"
exit 0
