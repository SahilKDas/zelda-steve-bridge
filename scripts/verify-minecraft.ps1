param([string]$Java = "$env:JAVA_HOME/bin/java.exe", [string]$Verifier)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $Verifier) { $Verifier="$root/build/minecraft_verify.exe" }
$runtime = "$root/runtime/minecraft"
if (Test-Path -LiteralPath "$runtime/stop.request") { Remove-Item -LiteralPath "$runtime/stop.request" }
$bridge = Start-Process "$root/build/bridge.exe" -ArgumentList default,300 -WindowStyle Hidden -PassThru
$server = $null
try {
    $arguments = '"{0}" "{1}" "{2}" 240' -f $Java,$runtime,"$root/build/libsteve_jni.dll"
    $server = Start-Process "$root/build/mc_supervisor.exe" -ArgumentList $arguments -WindowStyle Hidden `
        -RedirectStandardOutput "$runtime/resources.log" -RedirectStandardError "$runtime/supervisor-error.log" -PassThru
    & $Verifier
    if ($LASTEXITCODE -ne 0) { throw 'Real Minecraft integration test failed' }
} finally {
    if ($server -and -not $server.HasExited) {
        Set-Content -LiteralPath "$runtime/stop.request" -Value stop
        if (-not $server.WaitForExit(20000)) { Stop-Process -Id $server.Id; throw 'Server failed to shut down normally' }
    }
    if (-not $bridge.HasExited) { Stop-Process -Id $bridge.Id }
}
Get-Content -LiteralPath "$runtime/resources.log"
if ($server.ExitCode -ne 0) { throw 'Server resource supervisor reported failure' }
& "$PSScriptRoot/disk-budget.ps1"
