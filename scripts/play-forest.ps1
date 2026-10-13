param([string]$Java = "$env:JAVA_HOME/bin/java.exe")
$ErrorActionPreference='Stop'
$configPath=Join-Path $PSScriptRoot '../runtime/soh/shipofharkinian.json'
$config=Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json
$oldBoot=$config.CVars.gSettings.PSObject.Properties['BootSequence']
$oldBootValue=if($oldBoot){$oldBoot.Value}else{$null}
$oldWarps=$config.PSObject.Properties['WarpPoints']
$oldWarpValue=if($oldWarps){$oldWarps.Value}else{$null}
$config.CVars.gSettings | Add-Member -NotePropertyName BootSequence -NotePropertyValue 4 -Force
$config | Add-Member -NotePropertyName WarpPoints -NotePropertyValue @{
    'Forest playground'=@{entranceId=238;roomNum=0;pos=@{x=5.0;y=0.0;z=5.0};rotY=0;bootToPoint=$true}
} -Force
try {
    $config | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $configPath
    # SoH's boot-to-warp creates a temporary debug save; existing save files stay intact.
    & "$PSScriptRoot/run.ps1" -Java $Java
} finally {
    $latest=Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json
    if($oldBoot){$latest.CVars.gSettings | Add-Member -NotePropertyName BootSequence -NotePropertyValue $oldBootValue -Force}
    else{$latest.CVars.gSettings.PSObject.Properties.Remove('BootSequence')}
    if($oldWarps){$latest | Add-Member -NotePropertyName WarpPoints -NotePropertyValue $oldWarpValue -Force}
    else{$latest.PSObject.Properties.Remove('WarpPoints')}
    $latest | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $configPath
}
