# MavUI Windows Build Helper Script
# Run this inside Windows PowerShell (or Developer PowerShell for VS 2022)

param(
    [string]$BuildType = "Release",
    [string]$QtDir = "C:\Qt\6.8.3\msvc2022_64",
    [string]$GstDir = "C:\gstreamer\1.0\msvc_x86_64",
    [string]$BuildDir = "C:\build\mavui",
    [switch]$CreateInstaller = $false,
    [switch]$Clean = $false
)

$ErrorActionPreference = "Stop"

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "       MavUI Windows Build System        " -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan

# 0. Enable UNC path support in cmd.exe (Fix for CMD does not support UNC paths)
try {
    if (-not (Test-Path "HKCU:\Software\Microsoft\Command Processor")) {
        New-Item -Path "HKCU:\Software\Microsoft\Command Processor" -Force | Out-Null
    }
    Set-ItemProperty -Path "HKCU:\Software\Microsoft\Command Processor" -Name "DisableUNCCheck" -Value 1 -Type DWord -Force
} catch {
    # Ignore registry error if permission issue
}

# 1. Check Source Directory & Convert UNC to Drive Letter if needed
$SourceDir = $PSScriptRoot
if ($SourceDir.StartsWith("\\")) {
    $parts = $SourceDir.Split('\', [System.StringSplitOptions]::RemoveEmptyEntries)
    if ($parts.Length -ge 2) {
        $uncRoot = "\\$($parts[0])\$($parts[1])"
        $relPath = ($parts[2..($parts.Length - 1)]) -join '\'
        
        if (-not (Test-Path "Z:\")) {
            Write-Host "  -> Mapping network share $uncRoot to Z: drive..." -ForegroundColor Yellow
            & net use Z: "$uncRoot" /persistent:no | Out-Null
        }
        if (Test-Path "Z:\$relPath") {
            $SourceDir = "Z:\$relPath"
            Set-Location $SourceDir
        }
    }
}
Write-Host "[1/7] Source directory: $SourceDir"

# 2. Check Git (Required for CPM & FetchContent)
Write-Host "[2/7] Checking Git..."
$GitPaths = @("C:\Program Files\Git\cmd", "C:\Program Files\Git\bin", "$env:LOCALAPPDATA\Programs\Git\cmd")
foreach ($gp in $GitPaths) {
    if ((Test-Path $gp) -and ($env:PATH -notlike "*$gp*")) {
        $env:PATH = "$gp;$env:PATH"
    }
}
if (-not (Get-Command git.exe -ErrorAction SilentlyContinue)) {
    Write-Warning "Git bulunamadi! CPM bağımlılıklarını (ulog_cpp vb.) klonlamak için Git gereklidir."
} else {
    Write-Host "  -> Git bulundu: $(Get-Command git.exe | Select-Object -ExpandProperty Source)" -ForegroundColor Green
}

# 3. Check Visual Studio Environment
Write-Host "[3/7] Checking MSVC compiler..."
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsInstallPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vsInstallPath) {
            $vsDevCmd = Join-Path $vsInstallPath "Common7\Tools\Launch-VsDevShell.ps1"
            if (Test-Path $vsDevCmd) {
                Write-Host "  -> Loading VS Developer Environment..." -ForegroundColor Yellow
                & $vsDevCmd -Arch amd64 -HostArch amd64
            }
        }
    }
}

if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    Write-Warning "cl.exe (MSVC) bulunamadi! Lutfen Visual Studio 2022 C++ Build Tools yuklu oldugundan veya Developer PowerShell kullandiginizdan emin olun."
}

# 4. Check Qt Installation
Write-Host "[4/7] Checking Qt 6.8.3..."
if (-not (Test-Path "$QtDir\bin\qt-cmake.bat")) {
    $PossibleQt = @("C:\Qt\6.8.3\msvc2022_64", "C:\Qt\6.8.2\msvc2022_64", "C:\Qt\6.8.1\msvc2022_64")
    foreach ($p in $PossibleQt) {
        if (Test-Path "$p\bin\qt-cmake.bat") {
            $QtDir = $p
            break
        }
    }
}

if (-not (Test-Path "$QtDir\bin\qt-cmake.bat")) {
    Write-Error "Qt 6.8.x bulunamadi: $QtDir. Lutfen tools\setup\install-qt-windows.ps1 calistirin veya Qt dizinini -QtDir ile belirtin."
    exit 1
}
Write-Host "  -> Qt dizini: $QtDir" -ForegroundColor Green

# Add Qt to PATH
$env:PATH = "$QtDir\bin;$env:PATH"
$env:QT_ROOT_DIR = $QtDir

# 5. Check GStreamer
Write-Host "[5/7] Checking GStreamer..."
if (Test-Path $GstDir) {
    $env:GSTREAMER_1_0_ROOT_MSVC_X86_64 = $GstDir
    $env:PATH = "$GstDir\bin;$GstDir\lib;$env:PATH"
    Write-Host "  -> GStreamer bulundu: $GstDir" -ForegroundColor Green
} else {
    Write-Warning "GStreamer $GstDir bulunamadi. Video akisi gerekiyorsa tools\setup\install-dependencies-windows.ps1 calistirin."
}

# Check NSIS for packaging
$NsisDir = "${env:ProgramFiles(x86)}\NSIS"
if ((Test-Path $NsisDir) -and ($env:PATH -notlike "*$NsisDir*")) {
    $env:PATH = "$NsisDir;$env:PATH"
}

# 6. Prepare Build Directory
Write-Host "[6/7] Preparing build folder: $BuildDir"
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "  -> Cleaning entire build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir -ErrorAction SilentlyContinue
}
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}
# Always remove stale staging, deploy/windows cache, and RC compiled resources to force fresh icons
Remove-Item -Recurse -Force "$BuildDir\staging", "$BuildDir\deploy", "$BuildDir\*MavUI.rc*", "$BuildDir\*QGroundControl.rc*", "$BuildDir\*.exe" -ErrorAction SilentlyContinue

# 7. Configure & Build with CMake + Ninja
Write-Host "[7/7] Configuring CMake..." -ForegroundColor Cyan
Set-Location $BuildDir

$QtCmake = "$QtDir\bin\qt-cmake.bat"
$cmakeArgs = @(
    "-S", $SourceDir,
    "-B", $BuildDir,
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DQGC_STABLE_BUILD=ON"
)

& $QtCmake @cmakeArgs

if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configure basarisiz oldu!"
    exit $LASTEXITCODE
}

Write-Host "`n>>> Proje Derleniyor ($BuildType)..." -ForegroundColor Cyan
cmake --build . --target all --config $BuildType

if ($LASTEXITCODE -ne 0) {
    Write-Error "Derleme basarisiz oldu!"
    exit $LASTEXITCODE
}

Write-Host "`n=========================================" -ForegroundColor Green
Write-Host "   ✅ MavUI Basariyla Derlendi!         " -ForegroundColor Green
Write-Host "   Calistirilabilir dosya: $BuildDir\$BuildType\MavUI.exe" -ForegroundColor Green
Write-Host "=========================================" -ForegroundColor Green

# Optional: Installer Generation via NSIS
if ($CreateInstaller) {
    Write-Host "`n>>> Installer Paketleniyor (NSIS)..." -ForegroundColor Cyan
    cmake --install . --config $BuildType
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✅ Kurulum paketi olusturuldu!" -ForegroundColor Green
    }
}
