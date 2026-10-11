param([switch]$AcceptEula)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$runtime = Join-Path $root 'runtime/minecraft'
New-Item -ItemType Directory -Force -Path "$runtime/mods" | Out-Null
$mod = "$root/src/headless_mc/fabric/build/libs/steve-fabric-bridge-1.0.0.jar"
if (-not (Test-Path -LiteralPath $mod)) { throw 'Build the Fabric adapter first' }
Copy-Item -LiteralPath $mod -Destination "$runtime/mods/steve-fabric-bridge.jar"
foreach($module in 'fabric-api-base','fabric-lifecycle-events-v1') {
    $api = Get-ChildItem -LiteralPath "$root/cache/gradle-home/caches/modules-2/files-2.1/net.fabricmc.fabric-api/$module" -Filter '*.jar' -Recurse -File |
        Where-Object { $_.Name -notmatch 'sources' } | Select-Object -Last 1
    if (-not $api) { throw "$module jar missing from build cache" }
    Copy-Item -LiteralPath $api.FullName -Destination "$runtime/mods/$module.jar"
}
if (Test-Path -LiteralPath "$runtime/mods/fabric-api.jar") { Remove-Item -LiteralPath "$runtime/mods/fabric-api.jar" }
if ($AcceptEula) { Set-Content -LiteralPath "$runtime/eula.txt" -Value 'eula=true' }
if (-not (Test-Path -LiteralPath "$runtime/eula.txt")) { throw 'Pass -AcceptEula only after accepting https://www.minecraft.net/eula' }
if (-not (Test-Path -LiteralPath "$runtime/server.properties")) {
    Set-Content -LiteralPath "$runtime/server.properties" -Value @'
server-ip=127.0.0.1
server-port=25565
online-mode=false
enforce-secure-profile=false
enable-query=false
enable-rcon=false
view-distance=2
simulation-distance=2
max-players=1
pause-when-empty-seconds=-1
max-world-size=16
spawn-protection=0
level-name=bridge-world
level-type=minecraft:flat
generator-settings={"layers":[{"block":"minecraft:air","height":1}],"biome":"minecraft:the_void","structure_overrides":[]}
generate-structures=false
gamemode=survival
difficulty=normal
sync-chunk-writes=false
max-tick-time=60000
network-compression-threshold=-1
motd=Local Ocarina Steve logic bridge
'@
}
$bytes = [long](Get-ChildItem -LiteralPath $runtime -Force -Recurse -File | Measure-Object Length -Sum).Sum
Write-Output "Minecraft runtime bytes: $bytes / 200000000"
if ($bytes -ge 200000000) { throw 'Minecraft runtime exceeds 200 MB' }
& "$PSScriptRoot/disk-budget.ps1"
