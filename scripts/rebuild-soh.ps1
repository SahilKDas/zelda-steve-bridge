# Incremental build using the same dependency environment as build-soh.ps1.
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$vsPath=& "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe" -latest -products '*' -property installationPath
$envLines=& cmd.exe /d /c "`"$vsPath\Common7\Tools\VsDevCmd.bat`" -arch=x64 -host_arch=x64 >nul && set"
foreach($line in $envLines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($matches[1],$matches[2],'Process')}}
$env:VCPKG_ROOT="$root/vendor/vcpkg"
$env:VCPKG_DOWNLOADS="$root/cache/vcpkg-downloads"
$env:VCPKG_DEFAULT_BINARY_CACHE="$root/cache/vcpkg-packages"
$env:VCPKG_OVERLAY_TRIPLETS="$root/scripts/soh-triplets"
& "$vsPath/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" --build "$root/build/soh" --target soh -j 8
if($LASTEXITCODE -ne 0){throw 'SoH incremental build failed'}
& "$PSScriptRoot/disk-budget.ps1"
