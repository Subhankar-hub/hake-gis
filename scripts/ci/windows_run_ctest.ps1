#Requires -Version 5.1
<#
.SYNOPSIS
  Load the unbundled Windows build env and run CTest (ALL_BUT_PROVIDERS).

.DESCRIPTION
  Test binaries do not load hake-geodesk.env via mainwin.cpp. This script applies
  build/output/bin/hake-geodesk.env, prepends output bin/plugins to PATH, sets
  QT_QPA_PLATFORM=offscreen, then runs ctest excluding provider labels and
  blocklist/flaky names.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,

    [Parameter(Mandatory = $true)]
    [string]$RepoRoot,

    [Parameter(Mandatory = $false)]
    [string]$BlocklistFile = "",

    [Parameter(Mandatory = $false)]
    [string]$FlakyFile = "",

    [Parameter(Mandatory = $false)]
    [int]$Parallel = 0,

    [Parameter(Mandatory = $false)]
    [switch]$ListOnly
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Write-Section([string]$Title) {
    Write-Host ""
    Write-Host "==== $Title ===="
}

function Expand-CmakeEnvRefs([string]$Value) {
    # Dev env template may leave $ENV{WINDIR}-style tokens after configure_file.
    return [regex]::Replace($Value, '\$ENV\{([^}]+)\}', {
        param($m)
        $name = $m.Groups[1].Value
        $got = [Environment]::GetEnvironmentVariable($name, "Process")
        if (-not $got) {
            $got = [Environment]::GetEnvironmentVariable($name, "Machine")
        }
        if (-not $got) {
            $got = [Environment]::GetEnvironmentVariable($name, "User")
        }
        if ($got) { return $got }
        return $m.Value
    })
}

function Get-BuildEnvMap([string]$EnvFilePath) {
    if (-not (Test-Path -LiteralPath $EnvFilePath)) {
        throw "Missing build env file at $EnvFilePath (configure with WITH_VCPKG on Windows)"
    }

    $map = [ordered]@{}
    Get-Content -LiteralPath $EnvFilePath -Encoding UTF8 | ForEach-Object {
        $line = $_.TrimEnd("`r")
        if (-not $line -or $line.StartsWith("#")) { return }
        $eq = $line.IndexOf("=")
        if ($eq -lt 1) { return }
        $name = $line.Substring(0, $eq)
        $value = Expand-CmakeEnvRefs $line.Substring($eq + 1)
        $value = [Environment]::ExpandEnvironmentVariables($value)
        if ($name -ieq "PATH") {
            $old = [Environment]::GetEnvironmentVariable("PATH", "Process")
            if ($old) {
                $value = "$value;$old"
            }
        }
        $map[$name] = $value
    }
    return $map
}

function Apply-EnvMap([System.Collections.IDictionary]$Map) {
    foreach ($k in $Map.Keys) {
        Set-Item -Path "Env:$k" -Value $Map[$k]
        [Environment]::SetEnvironmentVariable($k, $Map[$k], "Process")
    }
}

function Get-ExcludeNames([string]$Path) {
    if (-not $Path -or -not (Test-Path -LiteralPath $Path)) {
        return @()
    }
    $names = @()
    Get-Content -LiteralPath $Path -Encoding UTF8 | ForEach-Object {
        $line = $_.Trim()
        if (-not $line -or $line.StartsWith("#")) { return }
        $names += $line
    }
    return $names
}

function Build-ExcludeRegex([string[]]$Names) {
    $list = @($Names | Where-Object { $_ })
    if ($list.Count -eq 0) {
        return ""
    }
    $escaped = @($list | ForEach-Object { [regex]::Escape($_) })
    # Match Linux docker-qgis-test.sh: ^name1$|^name2$
    return (($escaped | ForEach-Object { "^$_`$" }) -join "|")
}

if (-not $BlocklistFile) {
    $BlocklistFile = Join-Path $RepoRoot ".ci\test_blocklist_qt6_windows.txt"
}
if (-not $FlakyFile) {
    $FlakyFile = Join-Path $RepoRoot ".ci\test_flaky.txt"
}
if ($Parallel -le 0) {
    $Parallel = [Math]::Max(1, [int]$env:NUMBER_OF_PROCESSORS)
}

$BuildDir = [System.IO.Path]::GetFullPath($BuildDir)
$RepoRoot = [System.IO.Path]::GetFullPath($RepoRoot)
$outputDir = Join-Path $BuildDir "output"
$binDir = Join-Path $outputDir "bin"
$pluginsDir = Join-Path $outputDir "plugins"
$envFile = Join-Path $binDir "hake-geodesk.env"

Write-Section "Load build environment"
Write-Host "Env file: $envFile"
$envMap = Get-BuildEnvMap $envFile
Apply-EnvMap $envMap

# Ensure freshly built libs/plugins win over any stale PATH entries.
$env:PATH = "$binDir;$pluginsDir;$env:PATH"
$env:QT_QPA_PLATFORM = "offscreen"
$env:QTWEBENGINE_DISABLE_SANDBOX = "1"

Write-Host "QGIS_PREFIX_PATH=$env:QGIS_PREFIX_PATH"
Write-Host "PROJ_DATA=$env:PROJ_DATA"
Write-Host "GDAL_DATA=$env:GDAL_DATA"
Write-Host "PYTHONHOME=$env:PYTHONHOME"
Write-Host "QT_QPA_PLATFORM=$env:QT_QPA_PLATFORM"
$pathHead = @($env:PATH -split ';' | Select-Object -First 6) -join ';'
Write-Host "PATH(first)=$pathHead"

$excludeNames = @()
$excludeNames += Get-ExcludeNames $BlocklistFile
$excludeNames += Get-ExcludeNames $FlakyFile
$excludeNames = @($excludeNames | Select-Object -Unique)
$excludeRegex = Build-ExcludeRegex $excludeNames

Write-Section "CTest exclude list"
Write-Host "Blocklist: $BlocklistFile"
Write-Host "Flaky: $FlakyFile"
Write-Host "Exclude count: $($excludeNames.Count)"
if ($excludeRegex) {
    Write-Host "Exclude regex: $excludeRegex"
}

Push-Location $BuildDir
try {
    if ($ListOnly) {
        Write-Section "ctest -N (dry list)"
        & ctest -N -LE "HANA|POSTGRES|ORACLE|SQLSERVER"
        if ($LASTEXITCODE -ne 0) {
            throw "ctest -N failed with exit code $LASTEXITCODE"
        }
        return
    }

    Write-Section "Run CTest (ALL_BUT_PROVIDERS)"
    $ctestArgs = @(
        "-C", "Release",
        "-V",
        "--output-on-failure",
        "-LE", "HANA|POSTGRES|ORACLE|SQLSERVER",
        "--parallel", "$Parallel"
    )
    if ($excludeRegex) {
        $ctestArgs += @("-E", $excludeRegex)
    }

    Write-Host "ctest $($ctestArgs -join ' ')"
    & ctest @ctestArgs
    $code = $LASTEXITCODE
    if ($code -ne 0) {
        throw "ctest failed with exit code $code"
    }
    Write-Host "CTest completed successfully"
}
finally {
    Pop-Location
}
