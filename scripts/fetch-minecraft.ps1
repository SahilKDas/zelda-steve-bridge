param([switch]$AcceptEula)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $env:JAVA_HOME -or -not (Test-Path -LiteralPath "$env:JAVA_HOME/bin/java.exe")) { throw 'Set JAVA_HOME to an installed Java 21 JDK' }
New-Item -ItemType Directory -Force -Path "$root/cache","$root/runtime/minecraft" | Out-Null
$gradle = "$root/cache/gradle-9.2.1"
if (-not (Test-Path -LiteralPath "$gradle/bin/gradle.bat")) {
    $zip = "$root/cache/gradle-9.2.1-bin.zip"
    $uri = 'https://services.gradle.org/distributions/gradle-9.2.1-bin.zip'
    Invoke-WebRequest -Uri $uri -OutFile $zip
    $expected = (Invoke-RestMethod -Uri "$uri.sha256").ToString().Trim()
    if ((Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLower() -ne $expected.ToLower()) { throw 'Gradle checksum mismatch' }
    Expand-Archive -LiteralPath $zip -DestinationPath "$root/cache" -Force
    Remove-Item -LiteralPath $zip
}
$installer = "$root/cache/fabric-installer.jar"
$uri = 'https://maven.fabricmc.net/net/fabricmc/fabric-installer/1.1.1/fabric-installer-1.1.1.jar'
if (-not (Test-Path -LiteralPath $installer)) { Invoke-WebRequest -Uri $uri -OutFile $installer }
$expected = (Invoke-RestMethod -Uri "$uri.sha256").ToString().Trim().Split(' ')[0]
if ((Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash.ToLower() -ne $expected.ToLower()) { throw 'Fabric installer checksum mismatch' }
if (-not (Test-Path -LiteralPath "$root/runtime/minecraft/fabric-server-launch.jar")) {
    & "$env:JAVA_HOME/bin/java.exe" -jar $installer server -mcversion 1.21.11 -loader 0.19.5 -downloadMinecraft -dir "$root/runtime/minecraft"
    if ($LASTEXITCODE -ne 0) { throw 'Fabric server installation failed' }
}
if ($AcceptEula) { Set-Content -LiteralPath "$root/runtime/minecraft/eula.txt" -Value 'eula=true' }
& "$PSScriptRoot/build-minecraft.ps1"
