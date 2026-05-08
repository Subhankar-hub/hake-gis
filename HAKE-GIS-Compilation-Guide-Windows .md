# HAKE-GIS Windows Compilation Guide

**Complete step-by-step guide for compiling HAKE-GIS from source on Windows**

---

## Table of Contents

1. [Project Overview](#project-overview)
2. [Part 1: Windows Environment Setup](#part-1-windows-environment-setup)
3. [Part 2: Clone & Prepare HAKE-GIS Source](#part-2-clone-prepare-hake-gis-source)
4. [Part 3: CMake Configuration](#part-3-cmake-configuration)
5. [Part 4: Building HAKE-GIS](#part-4-building-hake-gis)
6. [Part 5: Installation](#part-5-installation)
7. [Part 6: Running HAKE-GIS](#part-6-running-hake-gis)
8. [Troubleshooting](#troubleshooting)
9. [Reference Commands](#reference-commands)

---

## Project Overview

| Property | Value |
|----------|-------|
| **Project** | HAKE GIS / QGIS 4.1.0 (Master Branch)  |
| **Source** | https://gitlab.haketech.com/Rohankar/hake-gis.git / https://github.com/qgis/QGIS/ |
| **Language** | C++ (C++20 standard) |
| **Build System** | CMake (3.22.0 minimum) |
| **Primary Dependencies** | Qt6 (6.8.3), GDAL, GEOS, Proj, GRASS, PostgreSQL/PostGIS |
| **Platform** | Windows 64-bit |
| **Estimated Build Time** | 2-4 hours |

---

## Part 1: Windows Environment Setup

### 1.1 Required Tools & Software

#### 1. Visual Studio 2026 Community Edition

- **Download**: https://visualstudio.microsoft.com/vs/community/
- **Required Workload**: Desktop Development with C++
- **Installation Steps**:
  1. Run the installer
  2. Select "Desktop Development with C++"
  3. Ensure MSVC v143 or later is selected (msvc_2022_x64 recommended)
  4. Complete the installation

#### 2. CMake (3.31.4 or later)

- **Download**: https://cmake.org/download/
- **File**: `cmake-3.31.4-windows-x86_64.msi` `cmake-4.2.1-windows-x86_64.msi`
- **Installation Steps**:
  1. Run the installer
  2. During installation, select "Add CMake to the system PATH for all users"
  3. Complete the installation
  4. **Verify**: Open Command Prompt and run:
     ```bash
     cmake --version
     ```

#### 3. Git (with Git for Windows)

- **Download**: https://git-scm.com/download/win
- **Installation**: Use default settings
- **Verify**: 
  ```bash
  git --version
  ```

#### 4. Flex and Bison for Windows

- **Download**: https://github.com/lexxmark/winflexbison/releases
- **File**: `win_flex_bison-2.5.25.zip`
- **Installation Steps**:
  1. Download and extract to `C:\winflexbison\`
  2. Verify installation:
     ```bash
     C:\winflexbison\win_bison.exe --version
     # Should output: bison (GNU Bison) 3.8.2
     ```
  3. Add to PATH (see Section 1.4)

#### 5. Ninja Build Tool

- **Download**: https://github.com/ninja-build/ninja/releases/download/v1.12.1/ninja-win.zip
- **Installation Steps**:
  1. Download and extract to `C:\ninja-win\`
  2. Add to PATH (see Section 1.4)
  3. **Verify**:
     ```bash
     ninja --version
     ```

#### 6. vcpkg

- **Installation Steps**:
  ```bash
  git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
  cd C:\vcpkg
  .\bootstrap-vcpkg.bat
  ```
- **Add to PATH**: `C:\vcpkg\installed\x64-windows\bin`
- **Verify**: 
  ```bash
  vcpkg version
  ```

#### 7. Python (3.11 or later)

- **Download**: https://www.python.org/downloads/ (Python 3.11 or 3.12)
- **Installation Steps**:
  1. Run the installer
  2. Select "Custom Installation"
  3. Check all options:
     - ✅ pip
     - ✅ tcl/tk and IDLE
     - ✅ Python test suite
     - ✅ py launcher
     - ✅ Install debug libraries
     - ✅ Install development headers
  4. Select "Add Python to PATH"
  5. Complete installation
- **Add to PATH**: `C:\Python311\` and `C:\Python311\Scripts\`
- **Verify**:
  ```bash
  python --version
  ```

#### 8. PostgreSQL (for PostGIS support)

- **Download**: https://www.postgresql.org/download/windows/
- **File**: `postgresql-18.1-1-windows-x64.exe`
- **Installation Steps**:
  1. Run the installer
  2. Set a superuser password (remember it!)
  3. Use default port 5432
  4. Complete installation
- **Verify**: pgAdmin should be available in Start Menu

---

### 1.2 Prepare Qt6 Environment

Choose one of the three installation methods below:

#### **Method A: Download Pre-Built Qt (RECOMMENDED for beginners)**

```bash
python -m pip install --upgrade pip aqtinstall
aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 -m all --outputdir C:\Qt
```

**Time**: ~30-45 minutes (depending on internet speed)

#### **Method B: Extract Pre-Built Qt Archive**

1. Download: https://download.qt.io/official_releases/qt/6.8/6.8.3/single/
2. File: `qt-everywhere-src-6.8.3.zip` (~1.2 GB)
3. Extract to `C:\Qt\`
4. Result: `C:\Qt\6.8.3\msvc2022_64\`

#### **Method C: Build Qt from Source**

```bash
git clone https://code.qt.io/qt/qt5.git -b v6.8.3 C:\Qt\qt6-6.8.3
cd C:\Qt\qt6-6.8.3
git submodule update --init --recursive
cmake -B build-release -S C:\Qt\qt6-6.8.3 -G "Ninja" `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64 `
  -DCMAKE_INSTALL_PREFIX=C:/Qt/6.8.3/msvc2022_64
cmake --build build-release --config Release --target install
```

**Time**: 3-6 hours

**Verification**: Check that this file exists:
```
C:\Qt\6.8.3\msvc2022_64\bin\qmake.exe
```

---

### 1.3 Prepare External Dependencies

#### 1.3.1 QCA (Qt Cryptographic Architecture)

```bash
git clone --branch v2.3.8 https://github.com/KDE/qca.git
cd qca
cmake -B build -S . -G "Visual Studio 18 2026" -A x64 `
  -DCMAKE_INSTALL_PREFIX=C:/qca `
  -DBUILD_WITH_QT6=ON `
  -DQCA_SUFFIX=qt6 `
  -DWITH_botan_PLUGIN=OFF `
  -DWITH_nss_PLUGIN=OFF `
  -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64 `
  -DQt6_DIR=C:/Qt/6.8.3/msvc2022_64/lib/cmake/Qt6
cmake --build build --config Release --target install
```

**Verify Installation**:
- `C:\qca\include\QtCrypto\qca.h` ✓
- `C:\qca\lib\qca-qt6.lib` ✓
- `C:\qca\lib\cmake\Qca-qt6\QcaConfig.cmake` ✓

**Add to PATH**: `C:\qca\bin`

---

#### 1.3.2 SpatialIndex

```bash
git clone https://github.com/libspatialindex/libspatialindex.git
cd libspatialindex
git checkout 1.9.3
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=C:/spatialindex-1.9.3
cmake --build build --config Release --target install
```

**Verify**: `C:\spatialindex-1.9.3\lib\spatialindex.lib` exists ✓

**Add to PATH**: `C:\spatialindex-1.9.3\bin`

---

#### 1.3.3 Vulkan SDK

- **Download**: https://vulkan.lunarg.com/sdk/home
- **File**: `vulkansdk-windows-x64-1.4.335.0.exe`
- **Installation**: Use default settings
- **Result**: Installed to `C:\VulkanSDK\1.4.335.0\`
- **Add to PATH**: `C:\VulkanSDK\1.4.335.0\Bin`

---

#### 1.3.4 Qt6Keychain

```bash
git clone https://github.com/frankosterfeld/qtkeychain.git
cd qtkeychain
cmake -B build -S . -G "Visual Studio 18 2026" -A x64 `
  -DCMAKE_INSTALL_PREFIX=C:/qtkeychain `
  -DBUILD_WITH_QT6=ON `
  -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64/lib/cmake
cmake --build build --config release --target install
```

**Verify**:
- `C:\qtkeychain\lib\cmake\Qt6Keychain\Qt6KeychainConfig.cmake` ✓
- `C:\qtkeychain\lib\qtkeychain.lib` ✓

**Add to PATH**: `C:\qtkeychain\bin`

---

#### 1.3.5 Core Libraries via vcpkg

```bash
vcpkg install `
  proj:x64-windows `
  gdal:x64-windows `
  geos:x64-windows `
  sqlite3:x64-windows `
  expat:x64-windows `
  tiff:x64-windows `
  zlib:x64-windows `
  zstd:x64-windows `
  pdal:x64-windows `
  gsl:x64-windows `
  exiv2:x64-windows `
  libpng:x64-windows `
  libjpeg-turbo:x64-windows `
  libpq:x64-windows `
  freexl:x64-windows `
  openssl:x64-windows `
  qwt:x64-windows `
  qscintilla:x64-windows `
  libzip:x64-windows
```

**Time**: 1-2 hours depending on internet speed

**Result**: Libraries installed to `C:\vcpkg\installed\x64-windows\`

---

#### 1.3.6 Python Build Tools

```bash
python -m pip install --upgrade pip
pip install sip PyQt6 PyQt6-sip pyqt-builder
```

**Verify**:
```bash
sip-build.exe --version
```

**Add to PATH**: `C:\Python311\Scripts\`

---

### 1.4 Set Environment Variables

#### **Consolidated PATH Variable**

Open **System Properties → Environment Variables** and add the following to your `PATH`:

```
C:\GIS\HAKE-GIS-Install\bin
C:\Qt\6.8.3\msvc2022_64\bin
C:\vcpkg\installed\x64-windows\bin
C:\winflexbison
C:\ninja-win
C:\qca\bin
C:\spatialindex-1.9.3\bin
C:\qtkeychain\bin
C:\Python311\Scripts
C:\Python311
C:\VulkanSDK\1.4.335.0\Bin
C:\Program Files\CMake\bin
```

#### **Individual Environment Variables (Optional for quick reference)**

Create these as System Variables for easier CMake configuration:

| Variable | Value |
|----------|-------|
| `QT_DIR` | `C:\Qt\6.8.3\msvc2022_64` |
| `VCPKG_ROOT` | `C:\vcpkg` |
| `VCPKG_TOOLCHAIN` | `C:\vcpkg\scripts\buildsystems\vcpkg.cmake` |
| `GDAL_DIR` | `C:\vcpkg\installed\x64-windows` |
| `GEOS_DIR` | `C:\vcpkg\installed\x64-windows` |
| `PROJ_DIR` | `C:\vcpkg\installed\x64-windows` |
| `QCA_DIR` | `C:\qca` |
| `SPATIALINDEX_PATH` | `C:\spatialindex-1.9.3` |
| `QTKEYCHAIN_DIR` | `C:\qtkeychain` |
| `Vulkan_DIR` | `C:\VulkanSDK\1.4.335.0` |

---

## Part 2: Clone & Prepare HAKE-GIS Source

### Step 1: Create Directory Structure

```bash
mkdir C:\GIS
cd C:\GIS
```

### Step 2: Clone HAKE-GIS Repository

```bash
git clone https://gitlab.haketech.com/Rohankar/hake-gis.git HAKE-GIS-Code
cd HAKE-GIS-Code
git branch -a
# Check for latest stable branch (if not using master)
```

### Step 3: Create Build Directory

```bash
cd C:\GIS
mkdir HAKE-GIS-Build
cd HAKE-GIS-Build
```

### Final Directory Structure

```
C:\GIS\
├── HAKE-GIS-Code\          (source code)
│   ├── CMakeLists.txt
│   ├── cmake\
│   ├── src\
│   ├── tests\
│   └── ...
├── HAKE-GIS-Build\         (build output)
│   ├── CMakeCache.txt
│   ├── build.ps1
│   ├── configure.ps1
│   └── ...
└── HAKE-GIS-Install\       (installation)
    ├── bin\
    ├── lib\
    └── ...
```

---

## Part 3: CMake Configuration

### Step 1: Open Command Prompt in Build Directory

```bash
cd C:\GIS\HAKE-GIS-Build
cmd.exe
```

### Step 2: Create CMake Configuration Script

Create a file named `configure.ps1` in `C:\GIS\HAKE-GIS-Build\`:

```powershell
# configure.ps1

$QGIS_SOURCE = "C:\GIS\HAKE-GIS-Code"
$QGIS_BUILD = "C:\GIS\HAKE-GIS-Build"
$QGIS_INSTALL = "C:\GIS\HAKE-GIS-Install"
$Qt_DIR = "C:\Qt\6.8.3\msvc2022_64"
$VCPKG_ROOT = "C:\vcpkg"
$VCPKG_TOOLCHAIN = "$VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
$SPATIALINDEX_PATH = "C:\spatialindex-1.9.3"

# Setup VS 2026
$VsPath = "C:\Program Files\Microsoft Visual Studio\18\Community"
$VsCmd = Join-Path $VsPath "VC\Auxiliary\Build\vcvars64.bat"

$EnvCmd = @"
@echo off
call "$VsCmd"
set
"@

$TempBat = "$env:TEMP\setup_vs.bat"
$EnvCmd | Out-File $TempBat -Encoding ASCII
$EnvOutput = & cmd /c $TempBat
Remove-Item $TempBat -Force

foreach ($Line in $EnvOutput) {
    if ($Line -match "^([^=]+)=(.*)$") {
        Set-Item -Path "env:$($matches[1])" -Value $matches[2] -Force | Out-Null
    }
}

Write-Host "Running CMake..." -ForegroundColor Yellow
Write-Host ""

# Run CMake configuration
cmake -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -S "$QGIS_SOURCE" `
  -B "$QGIS_BUILD" `
  -DCMAKE_INSTALL_PREFIX="$QGIS_INSTALL" `
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_TOOLCHAIN" `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DCMAKE_PREFIX_PATH="$Qt_DIR" `
  -DQt6_DIR="$Qt_DIR\lib\cmake\Qt6" `
  -DQt6Multimedia_DIR="$Qt_DIR\lib\cmake\Qt6Multimedia" `
  -DSPATIALINDEX_INCLUDE_DIR="$SPATIALINDEX_PATH\include" `
  -DSPATIALINDEX_LIBRARY="$SPATIALINDEX_PATH\lib\spatialindex.lib" `
  -DBISON_EXECUTABLE=C:\winflexbison\win_bison.exe `
  -DFLEX_EXECUTABLE=C:\winflexbison\win_flex.exe `
  -DQca_DIR=C:\qca\lib\cmake\Qca-qt6 `
  -DQt6Keychain_DIR=C:\qtkeychain\lib\cmake\Qt6Keychain `
  -DVulkan_INCLUDE_DIR=C:\VulkanSDK\1.4.335.0\Include `
  -DWITH_DESKTOP=ON `
  -DWITH_GUI=ON `
  -DWITH_CORE=ON `
  -DWITH_ANALYSIS=ON `
  -DWITH_AUTH=ON `
  -DWITH_QUICK=ON `
  -DWITH_SERVER=OFF `
  -DWITH_CRASH_HANDLER=ON

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "CMake Configuration SUCCESS!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Next step - Build with:" -ForegroundColor Cyan
    Write-Host "  cmake --build `"$QGIS_BUILD`" --parallel 4" -ForegroundColor White
}
else {
    Write-Host ""
    Write-Host "CMake Configuration FAILED" -ForegroundColor Red
    exit 1
}
```

### Step 3: Run CMake Configuration

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\GIS\HAKE-GIS-Build\configure.ps1
```

### Expected Output

```
Running CMake...

-- The C compiler identification is MSVC 19.x.x
-- The CXX compiler identification is MSVC 19.x.x
...
-- Configuring done
-- Generating done
-- Build files have been written to: C:\GIS\HAKE-GIS-Build

CMake Configuration SUCCESS!

Next step - Build with:
  cmake --build "C:\GIS\HAKE-GIS-Build" --parallel 4
```

---

## Part 4: Building HAKE-GIS

### Step 1: Create Build Script

Create a file named `build.ps1` in `C:\GIS\HAKE-GIS-Build\`:

```powershell
# build.ps1

$QGIS_BUILD = "C:\GIS\HAKE-GIS-Build"

Write-Host "Building HAKE-GIS..." -ForegroundColor Cyan
Write-Host "(Estimated: 2-4 hours)" -ForegroundColor Yellow
Write-Host ""

# Setup VS 2026
$VsPath = "C:\Program Files\Microsoft Visual Studio\18\Community"
$VsCmd = Join-Path $VsPath "VC\Auxiliary\Build\vcvars64.bat"

cmd /c """$VsCmd"" && cd /d ""$QGIS_BUILD"" && cmake --build . --config Release --parallel 4"

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "BUILD SUCCESSFUL!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Next step - Install:" -ForegroundColor Cyan
    Write-Host "  cmake --install `"$QGIS_BUILD`"" -ForegroundColor White
}
else {
    Write-Host ""
    Write-Host "BUILD FAILED" -ForegroundColor Red
    exit 1
}
```

### Step 2: Start Build Process

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\GIS\HAKE-GIS-Build\build.ps1
```

### Expected Output

```
Building HAKE-GIS...
(Estimated: 2-4 hours)

[1/2500] Linking CXX executable bin/qgis_bench.exe
[2/2500] Linking CXX executable bin/qgis.exe
...
[2500/2500] Built target qgis

BUILD SUCCESSFUL!

Next step - Install:
  cmake --install "C:\GIS\HAKE-GIS-Build"
```

---

## Part 5: Installation

### Step 1: Create Install Script

Create a file named `install.ps1` in `C:\GIS\HAKE-GIS-Build\`:

```powershell
# install.ps1

$QGIS_BUILD = "C:\GIS\HAKE-GIS-Build"

Write-Host "Installing HAKE-GIS..." -ForegroundColor Cyan
cmake --install "$QGIS_BUILD" --config Release

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "INSTALLATION SUCCESSFUL!" -ForegroundColor Green
    Write-Host ""
    Write-Host "HAKE-GIS is installed to: C:\GIS\HAKE-GIS-Install" -ForegroundColor Green
}
else {
    Write-Host ""
    Write-Host "INSTALLATION FAILED" -ForegroundColor Red
    exit 1
}
```

### Step 2: Run Installation

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\GIS\HAKE-GIS-Build\install.ps1
```

### Step 3: Verify Installation

```bash
dir C:\GIS\HAKE-GIS-Install\bin\qgis.exe
# Should show: qgis.exe file
```

---

## Part 6: Running HAKE-GIS

### Method 1: Direct Launch

```bash
C:\GIS\HAKE-GIS-Install\bin\qgis.exe
```

### Method 2: Command Line

```bash
cd C:\GIS\HAKE-GIS-Install\bin
qgis.exe
```

### Method 3: Create Desktop Shortcut

1. Right-click Desktop
2. Select "New" → "Shortcut"
3. Location: `C:\GIS\HAKE-GIS-Install\bin\qgis.exe`
4. Name: "HAKE-GIS"
5. Click Finish

### Expected Startup

```
Starting HAKE-GIS...
Loading plugins...
Initializing GUI...
[HAKE-GIS window opens]
```

---

## Troubleshooting

### CMake Configuration Fails

**Error**: `Could not find Qt6...`

**Solution**:
```powershell
# Verify Qt6 installation
dir C:\Qt\6.8.3\msvc2022_64\lib\cmake\Qt6
# Should contain: Qt6Config.cmake

# If missing, reinstall Qt6 or verify path in configure.ps1
```

---

**Error**: `Could not find GDAL...`

**Solution**:
```bash
# Verify vcpkg installation
vcpkg list
# Check if gdal:x64-windows is installed

# If missing:
vcpkg install gdal:x64-windows
```

---

### Build Fails During Compilation

**Error**: `Linker error: undefined reference...`

**Solution**:
1. Check all dependencies are in PATH (see Section 1.4)
2. Clear build directory and reconfigure:
   ```bash
   cd C:\GIS\HAKE-GIS-Build
   rm -r * (or del * for Windows)
   # Re-run configure.ps1
   ```
3. Ensure all vcpkg packages are installed

---

**Error**: `Out of memory during build`

**Solution**:
1. Reduce parallel builds:
   ```bash
   cmake --build . --config Release --parallel 2
   ```
2. Close other applications (Chrome, Visual Studio, etc.)
3. Try building at night when system resources are available

---

### HAKE-GIS Crashes on Startup

**Error**: `Application failed to start`

**Solution**:
1. Verify all DLLs are available:
   ```bash
   where qgis.exe
   # Add all dependency directories to PATH
   ```

---

## Reference Commands

### CMake Commands

| Command | Purpose |
|---------|---------|
| `cmake --version` | Check CMake version |
| `cmake -G Ninja -S source -B build ...` | Configure with Ninja |
| `cmake --build . --config Release --parallel 4` | Build with 4 threads |
| `cmake --install . --config Release` | Install HAKE-GIS |


### vcpkg Commands

| Command | Purpose |
|---------|---------|
| `vcpkg integrate install` | Integrate with Visual Studio |
| `vcpkg list` | List installed packages |
| `vcpkg install package:x64-windows` | Install a package |
| `vcpkg remove package:x64-windows` | Remove a package |
| `vcpkg search keyword` | Search for packages |
| `vcpkg update` | Update vcpkg |

### PowerShell Commands

| Command | Purpose |
|---------|---------|
| `powershell -NoProfile -ExecutionPolicy Bypass -File script.ps1` | Run PS script |
| `$env:PATH` | Check PATH variable |
| `Get-Location` | Get current directory |
| `cd path` | Change directory |

### Useful Windows Commands

| Command | Purpose |
|---------|---------|
| `setx VAR_NAME value` | Set user environment variable |
| `setx VAR_NAME value /M` | Set system environment variable |
| `set` | Show all environment variables |
| `echo %VAR_NAME%` | Display variable value |

---

## Quick Start Workflow

### Complete build in 5 minutes:

```bash
# 1. Navigate to build directory
cd C:\GIS\HAKE-GIS-Build

# 2. Configure
powershell -NoProfile -ExecutionPolicy Bypass -File .\configure.ps1

# 3. Build (go get coffee - takes 2-4 hours)
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1

# 4. Install
powershell -NoProfile -ExecutionPolicy Bypass -File .\install.ps1

# 5. Run HAKE-GIS
C:\GIS\HAKE-GIS-Install\bin\qgis.exe
```

---

## Summary Checklist

- [ ] Visual Studio 2026 Community installed
- [ ] CMake 3.31.4+ installed and in PATH
- [ ] Git installed
- [ ] Flex/Bison installed and in PATH
- [ ] Ninja installed and in PATH
- [ ] vcpkg installed and bootstrapped
- [ ] Python 3.11+ installed with PyQt6
- [ ] PostgreSQL 18+ installed
- [ ] Qt 6.8.3 installed (via one of three methods)
- [ ] QCA built and installed
- [ ] SpatialIndex built and installed
- [ ] Vulkan SDK installed
- [ ] Qt6Keychain built and installed
- [ ] vcpkg packages installed (19+ libraries)
- [ ] All PATHs and environment variables set
- [ ] HAKE-GIS source code cloned
- [ ] Build directory created
- [ ] CMake configuration successful
- [ ] Build completed successfully
- [ ] Installation completed successfully
- [ ] HAKE-GIS launched and verified

---
