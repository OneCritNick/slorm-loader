param([Parameter(Mandatory=$true)][string]$ArchivePath,[Parameter(Mandatory=$true)][string]$NexusArchivePath,[string]$VCToolsPath,[string]$WindowsSdkRoot,[string]$SdkVersion='10.0.22621.0')
$ErrorActionPreference='Stop';$root=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'toolchain.ps1');$tc=Initialize-SlormToolchain $VCToolsPath $WindowsSdkRoot $SdkVersion
$stage=Join-Path $root ('build\package-test-'+[Guid]::NewGuid().ToString('N'));$unpacked=Join-Path $stage 'unpacked'
New-Item -ItemType Directory -Path $stage|Out-Null
Expand-Archive -LiteralPath $ArchivePath -DestinationPath $unpacked
foreach($name in @('tools/slorm-loader-setup.js','tools/bootstrap.js','tools/strict-json.js','slorm-runtime.json','slorm-loader-checksums.json','version.dll','version.ini','plugins\AurieCore.asi','mods\Aurie\YYToolkit.dll','mods\Aurie\SlormLoader.dll','source\build.ps1','docs\LICENSE-AGPL.txt','docs\LICENSE-Ultimate-ASI-Loader.txt','docs\RELEASE-NOTES.md','docs\SECURITY.md')){
    if(-not(Test-Path -LiteralPath "$unpacked\$name" -PathType Leaf)){throw "Packaged runtime file missing: $name"}
}
foreach($dir in @('vortex-extension','mods','exports')){if(Test-Path -LiteralPath "$unpacked\source\$dir"){throw "Unrelated project in package source: $dir"}}
if((Test-Path -LiteralPath "$unpacked\mods\Slormancer") -and @(Get-ChildItem -LiteralPath "$unpacked\mods\Slormancer" -Recurse -Filter mod.json).Count){throw 'Runtime package enables a gameplay mod'}
if((Get-FileHash "$unpacked\mods\Aurie\YYToolkit.dll").Hash.ToLowerInvariant() -ne 'ae7809f136f9222e5f49393d7ce7c3ad4c375c171308e2414660946d4ba0378a'){throw 'Packaged YYToolkit does not match the accepted release'}
$sourcePins=Get-Content "$unpacked\docs\source-checksums.json" -Raw|ConvertFrom-Json
foreach($pin in $sourcePins.PSObject.Properties){if((Get-FileHash -LiteralPath "$unpacked\source\$($pin.Name)").Hash.ToLowerInvariant() -ne $pin.Value){throw "Source archive hash differs: $($pin.Name)"}}
Invoke-SlormChecked 'node' @("$unpacked\source\tools\verify-source.js")
Invoke-SlormChecked 'node' @("$root\tests\package-bootstrap.test.cjs",$unpacked,"$stage\transactions")
$nexus=Join-Path $stage 'nexus'
Expand-Archive -LiteralPath $NexusArchivePath -DestinationPath $nexus
Invoke-SlormChecked 'node' @("$root\tests\package-distributions.test.cjs",$unpacked,$nexus)
Invoke-SlormChecked 'node' @("$root\tests\package-bootstrap.test.cjs",$nexus,"$stage\nexus-transactions")
[ordered]@{archiveSha256=(Get-FileHash $ArchivePath).Hash.ToLowerInvariant();nexusArchiveSha256=(Get-FileHash $NexusArchivePath).Hash.ToLowerInvariant();sourceFilesChecked=@($sourcePins.PSObject.Properties).Count;identicalRuntimeFiles=$true;leanPlayerAllowlist=$true;officialYYToolkit=$true;packageInstallRepeatRestore=$true;tamperedCoreRejected=$true;noBundledExe=$true;gameLaunched=$false;result='passed'}|ConvertTo-Json|Set-Content -LiteralPath "$root\build\package-results.json" -Encoding utf8
Write-Output 'PASS both EXE-free ZIPs, complete source hashes, identical player files and JavaScript setup/removal; no game launched.'
