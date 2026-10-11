param([int]$Seconds = 3600)
$ErrorActionPreference = 'Stop'
& "$PSScriptRoot/disk-budget.ps1"
$projectPath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$bridgeProcess = Start-Process -FilePath "$projectPath/build/bridge.exe" -ArgumentList 'default',$Seconds -WindowStyle Hidden -PassThru
try {
    & "$projectPath/build/headless_mc.exe" default $Seconds
    if ($LASTEXITCODE -ne 0) { throw 'Headless backend failed' }
} finally {
    if (-not $bridgeProcess.HasExited) { Stop-Process -Id $bridgeProcess.Id }
}
