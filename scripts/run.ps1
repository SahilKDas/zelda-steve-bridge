param([int]$Seconds = 86400, [string]$Java = "$env:JAVA_HOME/bin/java.exe", [switch]$BackendOnly)
$ErrorActionPreference = 'Stop'
& "$PSScriptRoot/disk-budget.ps1"
$projectPath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not (Test-Path -LiteralPath $Java)) { throw 'Set JAVA_HOME to Java 21 or pass -Java with its java.exe path' }
$runtime = "$projectPath/runtime/minecraft"
if (Test-Path -LiteralPath "$runtime/stop.request") { Remove-Item -LiteralPath "$runtime/stop.request" }
foreach ($path in 'build/bridge.exe','build/mc_supervisor.exe','build/libsteve_jni.dll','runtime/minecraft/fabric-server-launch.jar') {
    if (-not (Test-Path -LiteralPath "$projectPath/$path")) { throw "Missing $path; build and set up the project first" }
}
if (-not $BackendOnly -and -not (Test-Path -LiteralPath "$projectPath/runtime/soh/soh.exe")) { throw 'Run setup-soh.ps1 with your ROM first' }
$bridgeProcess = Start-Process -FilePath "$projectPath/build/bridge.exe" -ArgumentList 'default',$Seconds -WindowStyle Hidden -PassThru
$serverProcess = $null
$gameProcess = $null
try {
    Start-Sleep -Milliseconds 200
    if ($bridgeProcess.HasExited) { throw 'Bridge mapping already exists or failed to start; close the previous session first' }
    if (Test-Path -LiteralPath "$runtime/server-console.log") { Remove-Item -LiteralPath "$runtime/server-console.log" }
    $arguments = '"{0}" "{1}" "{2}" {3}' -f $Java,$runtime,"$projectPath/build/libsteve_jni.dll",$Seconds
    $serverProcess = Start-Process -FilePath "$projectPath/build/mc_supervisor.exe" -ArgumentList $arguments -WindowStyle Hidden `
        -RedirectStandardOutput "$runtime/resources.log" -RedirectStandardError "$runtime/supervisor-error.log" -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds(180)
    do {
        if ($serverProcess.HasExited) { throw 'Minecraft failed; see runtime/minecraft/server-console.log' }
        $ready = (Test-Path -LiteralPath "$runtime/server-console.log") -and `
            (Select-String -LiteralPath "$runtime/server-console.log" -Pattern 'STEVE_BRIDGE_READY' -Quiet)
        if (-not $ready) { Start-Sleep -Milliseconds 500 }
    } until ($ready -or [DateTime]::UtcNow -ge $deadline)
    if (-not $ready) { throw 'Minecraft startup timed out' }
    if ($BackendOnly) { $serverProcess.WaitForExit() }
    else {
        $gameProcess = Start-Process -FilePath "$projectPath/runtime/soh/soh.exe" -WorkingDirectory "$projectPath/runtime/soh" -PassThru
        while (-not $gameProcess.HasExited) {
            if ($serverProcess.HasExited) { throw 'Minecraft stopped; see its logs' }
            Start-Sleep -Milliseconds 500
        }
    }
} finally {
    if ($serverProcess -and -not $serverProcess.HasExited) {
        Set-Content -LiteralPath "$runtime/stop.request" -Value 'stop'
        if (-not $serverProcess.WaitForExit(20000)) { Stop-Process -Id $serverProcess.Id }
    }
    if (-not $bridgeProcess.HasExited) { Stop-Process -Id $bridgeProcess.Id }
}
