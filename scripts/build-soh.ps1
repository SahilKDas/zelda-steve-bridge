param([switch]$ConfigureOnly, [int]$Jobs = 8)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
& "$PSScriptRoot/disk-budget.ps1" -CleanObjects
if(-not (Test-Path -LiteralPath "$root/src/soh_mod/generated/minecraft_textures.inc")) {
    & python "$PSScriptRoot/fetch-minecraft-assets.py"
    if($LASTEXITCODE -ne 0) { throw 'Minecraft texture preparation failed' }
}
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
$sources = @()
foreach ($entry in (Get-Content -LiteralPath "$PSScriptRoot/soh-dependencies.json" -Raw | ConvertFrom-Json)) {
    $path = "$root/build/soh/_deps/$($entry.name)-src"
    if (-not (Test-Path -LiteralPath "$path/.git")) { throw 'Run fetch-soh.ps1 first to obtain shallow, pinned dependencies' }
    $sources += "-DFETCHCONTENT_SOURCE_DIR_$($entry.name.ToUpper())=$path"
}
& $cmakePath -S "$root/vendor/shipwright" -B "$root/build/soh" -G Ninja -DCMAKE_BUILD_TYPE=Release `
    -DSTEVE_BRIDGE_ROOT="$root" -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -DSTEVE_BUILD_JNI=OFF `
    -DCMAKE_CONFIGURATION_TYPES=Release -DENABLE_SCRIPTING=OFF -DVCPKG_ROOT="$root/vendor/vcpkg" @sources
if ($LASTEXITCODE -ne 0) { throw 'SoH configure failed' }
if (-not $ConfigureOnly) {
    & $cmakePath --build "$root/build/soh" --target soh GenerateSohOtr soh-torch -j $Jobs
    if ($LASTEXITCODE -ne 0) { throw 'SoH build failed' }
}
& "$PSScriptRoot/disk-budget.ps1"
