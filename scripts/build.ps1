$ErrorActionPreference = 'Stop'
& "$PSScriptRoot/disk-budget.ps1" -CleanObjects
$projectPath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
cmake -S $projectPath -B "$projectPath/build" -G 'MinGW Makefiles' -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw 'Configure failed' }
cmake --build "$projectPath/build" -j 4
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
ctest --test-dir "$projectPath/build" --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
& "$PSScriptRoot/disk-budget.ps1"
