$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $env:JAVA_HOME) { throw 'Set JAVA_HOME to an installed Java 21 JDK' }
$env:GRADLE_USER_HOME = "$root/cache/gradle-home"
& "$PSScriptRoot/disk-budget.ps1"
& "$root/cache/gradle-9.2.1/bin/gradle.bat" -p "$root/src/headless_mc/fabric" build --no-daemon --console=plain
if ($LASTEXITCODE -ne 0) { throw 'Fabric build failed' }
& "$PSScriptRoot/setup-minecraft.ps1"
