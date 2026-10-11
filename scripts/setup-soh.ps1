param([Parameter(Mandatory=$true)][string[]]$Rom)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$runtime = Join-Path $root 'runtime/soh'
$tool = Join-Path $root 'build/soh/soh-torch.exe'
if (-not (Test-Path -LiteralPath $tool)) { throw 'Build SoH and soh-torch first' }
New-Item -ItemType Directory -Force -Path $runtime | Out-Null
foreach ($path in $Rom) {
    $resolved = (Resolve-Path -LiteralPath $path).Path
    & $tool --src "$root/vendor/shipwright/soh/assets/yml" --dest $runtime --version 9.3.0 $resolved
    if ($LASTEXITCODE -ne 0) { throw "ROM extraction failed: $resolved" }
}
$exe = "$root/vendor/shipwright/x64/Release/soh.exe"
if (-not (Test-Path -LiteralPath $exe)) { throw 'Compiled soh.exe is missing' }
Copy-Item -LiteralPath $exe -Destination "$runtime/soh.exe"
Copy-Item -LiteralPath "$root/build/soh/soh/soh.o2r" -Destination "$runtime/soh.o2r"
if (Test-Path -LiteralPath "$root/build/soh/gamecontrollerdb.txt") {
    Copy-Item -LiteralPath "$root/build/soh/gamecontrollerdb.txt" -Destination "$runtime/gamecontrollerdb.txt"
}
$strip = Get-Command strip.exe -ErrorAction SilentlyContinue
if (-not $strip) { throw 'Put the MSYS2 UCRT64 bin directory on PATH so the Release executable can be stripped' }
& $strip.Source --strip-unneeded "$runtime/soh.exe"
if ($LASTEXITCODE -ne 0) { throw 'Release symbol stripping failed' }
if (-not (Test-Path -LiteralPath "$runtime/shipofharkinian.json")) {
    '{"CVars":{"gSettings":{"InterpolationFPS":60},"gEnhancements":{"SteveBridge":1}}}' |
        Set-Content -LiteralPath "$runtime/shipofharkinian.json"
}
if (Test-Path -LiteralPath "$runtime/torch.hash.yml") { Remove-Item -LiteralPath "$runtime/torch.hash.yml" }
& "$PSScriptRoot/disk-budget.ps1"
