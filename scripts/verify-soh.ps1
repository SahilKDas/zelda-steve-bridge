param([string]$Java = "$env:JAVA_HOME/bin/java.exe", [switch]$KeepOpen)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$test = "$root/runtime/soh-test"
New-Item -ItemType Directory -Force -Path $test | Out-Null
foreach($file in 'soh.exe','soh.o2r','oot.o2r') {
    Copy-Item -LiteralPath "$root/runtime/soh/$file" -Destination "$test/$file"
}
New-Item -ItemType Directory -Force -Path "$test/minecraft-assets" | Out-Null
Get-ChildItem -LiteralPath "$root/cache/minecraft-assets" | Copy-Item -Destination "$test/minecraft-assets" -Recurse -Force
# A temporary debug save and isolated configuration keep user saves untouched.
@'
{
  "CVars": {
    "gSettings": { "BootSequence": 4, "InterpolationFPS": 60 },
    "gEnhancements": { "SteveBridge": 1 }
  },
  "WarpPoints": {
    "Bridge integration arena": {
      "entranceId": 238, "roomNum": 0,
      "pos": { "x": 0.0, "y": 0.0, "z": 0.0 },
      "rotY": 0, "bootToPoint": true
    }
  }
}
'@ | Set-Content -LiteralPath "$test/shipofharkinian.json"
if(Test-Path -LiteralPath "$test/steve-in-game-test.log") { Remove-Item -LiteralPath "$test/steve-in-game-test.log" }
$oldTest=$env:STEVE_IN_GAME_TEST
$oldExit=$env:STEVE_IN_GAME_TEST_EXIT
$playerFile="$root/runtime/minecraft/bridge-world/steve-player.json"
$savedPlayer=if(Test-Path -LiteralPath $playerFile) { [IO.File]::ReadAllBytes($playerFile) } else { $null }
if(Test-Path -LiteralPath $playerFile) { Remove-Item -LiteralPath $playerFile }
try {
    $env:STEVE_IN_GAME_TEST='1'
    if(-not $KeepOpen) { $env:STEVE_IN_GAME_TEST_EXIT='1' }
    else { Remove-Item Env:STEVE_IN_GAME_TEST_EXIT -ErrorAction SilentlyContinue }
    & "$PSScriptRoot/run.ps1" -Java $Java -GameDirectory $test
} finally {
    $env:STEVE_IN_GAME_TEST=$oldTest
    $env:STEVE_IN_GAME_TEST_EXIT=$oldExit
    if($null -ne $savedPlayer) { [IO.File]::WriteAllBytes($playerFile,$savedPlayer) }
    elseif(Test-Path -LiteralPath $playerFile) { Remove-Item -LiteralPath $playerFile }
}
$result=Get-Content -LiteralPath "$test/steve-in-game-test.log" -Raw
Write-Output $result
if(-not $result.StartsWith('PASS ')) { throw 'Native SoH integration verification failed' }
& "$PSScriptRoot/disk-budget.ps1"
