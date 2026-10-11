param([switch]$CleanObjects)
$ErrorActionPreference = 'Stop'
$workspacePath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$projectPath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
function Measure-Workspace {
    return [long](Get-ChildItem -LiteralPath $workspacePath -Force -Recurse -File |
        Measure-Object -Property Length -Sum).Sum
}
$bytes = Measure-Workspace
if ($bytes -ge 4000000000 -and $CleanObjects) {
    $objectsPath = [IO.Path]::GetFullPath((Join-Path $projectPath 'build/CMakeFiles'))
    if (-not $objectsPath.StartsWith($projectPath + [IO.Path]::DirectorySeparatorChar)) {
        throw 'Cleanup path escaped project'
    }
    if (Test-Path -LiteralPath $objectsPath) {
        Get-ChildItem -LiteralPath $objectsPath -Recurse -File |
            Where-Object { $_.Extension -in '.obj','.o' } |
            ForEach-Object { Remove-Item -LiteralPath $_.FullName }
    }
    $bytes = Measure-Workspace
}
Write-Output "Workspace logical bytes (including hidden Git files): $bytes / 5000000000"
if ($bytes -ge 5000000000) { throw 'Workspace exceeds the 5.0 GB budget' }
if ($bytes -ge 4000000000) { Write-Warning 'Workspace is above 4.0 GB; stop fetching dependencies and clear build intermediates.' }
