$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
function Fetch-Pinned([string]$path,[string]$url,[string]$commit) {
    if (-not (Test-Path -LiteralPath "$path/.git")) {
        New-Item -ItemType Directory -Force -Path $path | Out-Null
        & git -C $path init
        & git -C $path remote add origin $url
        & git -C $path fetch --depth 1 origin $commit
        if ($LASTEXITCODE -ne 0) { throw "Cannot fetch $url at $commit" }
        & git -C $path checkout --detach FETCH_HEAD
    }
    if ((& git -C $path rev-parse HEAD) -ne $commit) { throw "Unexpected revision in $path" }
}
Fetch-Pinned "$root/vendor/shipwright" 'https://github.com/HarbourMasters/Shipwright.git' '7be5a4feacd1db1474bec97bb3ba7ad08de89d28'
& git -C "$root/vendor/shipwright" submodule update --init --depth 1 libultraship torch
if ($LASTEXITCODE -ne 0) { throw 'Submodule fetch failed' }
& git -C "$root/vendor/shipwright" apply --reverse --check "$root/src/soh_mod/shipwright.patch" 2>$null
if ($LASTEXITCODE -ne 0) {
    & git -C "$root/vendor/shipwright" apply "$root/src/soh_mod/shipwright.patch"
    if ($LASTEXITCODE -ne 0) { throw 'Cannot apply the pinned SoH integration patch' }
}
& git -C "$root/vendor/shipwright/libultraship" apply --reverse --check "$root/src/soh_mod/libultraship.patch" 2>$null
if ($LASTEXITCODE -ne 0) {
    & git -C "$root/vendor/shipwright/libultraship" apply "$root/src/soh_mod/libultraship.patch"
    if ($LASTEXITCODE -ne 0) { throw 'Cannot apply the native terrain renderer patch' }
}
Fetch-Pinned "$root/vendor/vcpkg" 'https://github.com/microsoft/vcpkg.git' 'e456309491fd875487bf02b8a0dca76c1b1b00cc'
& git -C "$root/vendor/vcpkg" checkout --detach HEAD
foreach ($entry in (Get-Content -LiteralPath "$PSScriptRoot/soh-dependencies.json" -Raw | ConvertFrom-Json)) {
    Fetch-Pinned "$root/build/soh/_deps/$($entry.name)-src" $entry.url $entry.commit
}
$imgui = "$root/build/soh/_deps/imgui-src"
$patch = "$root/vendor/shipwright/libultraship/cmake/dependencies/patches/imgui-fixes-and-config.patch"
& git -C $imgui apply --reverse --check $patch 2>$null
if ($LASTEXITCODE -ne 0) {
    & git -C $imgui apply $patch
    if ($LASTEXITCODE -ne 0) { throw 'ImGui integration patch failed' }
}
& "$PSScriptRoot/disk-budget.ps1"
