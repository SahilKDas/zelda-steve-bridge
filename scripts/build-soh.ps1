param([switch]$ConfigureOnly)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
& "$PSScriptRoot/disk-budget.ps1" -CleanObjects
$vsPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products '*' -property installationPath
if (-not $vsPath) { throw 'Visual Studio C++ build tools are required' }
$envLines = & cmd.exe /d /c "`"$vsPath\Common7\Tools\VsDevCmd.bat`" -arch=x64 -host_arch=x64 >nul && set"
foreach ($line in $envLines) {
    if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process') }
}
$cmakePath = Join-Path $vsPath 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$env:VCPKG_OVERLAY_TRIPLETS = "$PSScriptRoot/soh-triplets"
$env:VCPKG_DOWNLOADS = "$root/cache/vcpkg-downloads"
$env:VCPKG_DEFAULT_BINARY_CACHE = "$root/cache/vcpkg-packages"
$env:VCPKG_MAX_CONCURRENCY = '4'
$env:VCPKG_ROOT = "$root/vendor/vcpkg"
New-Item -ItemType Directory -Force -Path $env:VCPKG_DOWNLOADS,$env:VCPKG_DEFAULT_BINARY_CACHE | Out-Null
& $cmakePath -S "$root/vendor/shipwright" -B "$root/build/soh" -G Ninja -DCMAKE_BUILD_TYPE=Release `
    -DSTEVE_BRIDGE_ROOT="$root" -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -DSTEVE_BUILD_JNI=OFF `
    -DCMAKE_CONFIGURATION_TYPES=Release -DENABLE_SCRIPTING=OFF -DVCPKG_ROOT="$root/vendor/vcpkg"
if ($LASTEXITCODE -ne 0) { throw 'SoH configure failed' }
if (-not $ConfigureOnly) {
    & $cmakePath --build "$root/build/soh" --target soh GenerateSohOtr -j 4
    if ($LASTEXITCODE -ne 0) { throw 'SoH build failed' }
}
& "$PSScriptRoot/disk-budget.ps1"
