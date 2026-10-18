param([Parameter(Mandatory=$true)][string]$ArchivePath,[string]$VCToolsPath,[string]$WindowsSdkRoot,[string]$SdkVersion='10.0.22621.0')
$ErrorActionPreference='Stop';$root=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'toolchain.ps1');$tc=Initialize-SlormToolchain $VCToolsPath $WindowsSdkRoot $SdkVersion
$stage=Join-Path $root ('build\package-test-'+[Guid]::NewGuid().ToString('N'));$unpacked=Join-Path $stage 'unpacked'
New-Item -ItemType Directory -Path $stage|Out-Null
Expand-Archive -LiteralPath $ArchivePath -DestinationPath $unpacked
foreach($name in @('SlormLoaderSetup.exe','slorm-runtime.json','slorm-loader-checksums.json','version.dll','version.ini','plugins\AurieCore.asi','mods\Aurie\YYToolkit.dll','mods\Aurie\SlormLoader.dll','source\build.ps1','docs\LICENSE-AGPL.txt','docs\LICENSE-Ultimate-ASI-Loader.txt','docs\RELEASE-NOTES.md','docs\SECURITY.md')){
    if(-not(Test-Path -LiteralPath "$unpacked\$name" -PathType Leaf)){throw "Packaged runtime file missing: $name"}
}
foreach($dir in @('vortex-extension','mods','exports')){if(Test-Path -LiteralPath "$unpacked\source\$dir"){throw "Unrelated project in package source: $dir"}}
if(@(Get-ChildItem -LiteralPath "$unpacked\mods\Slormancer" -Recurse -Filter mod.json).Count){throw 'Runtime package enables a gameplay mod'}
if((Get-FileHash "$unpacked\mods\Aurie\YYToolkit.dll").Hash.ToLowerInvariant() -ne 'ae7809f136f9222e5f49393d7ce7c3ad4c375c171308e2414660946d4ba0378a'){throw 'Packaged YYToolkit does not match the accepted release'}
$sourcePins=Get-Content "$unpacked\docs\source-checksums.json" -Raw|ConvertFrom-Json
foreach($pin in $sourcePins.PSObject.Properties){if((Get-FileHash -LiteralPath "$unpacked\source\$($pin.Name)").Hash.ToLowerInvariant() -ne $pin.Value){throw "Source archive hash differs: $($pin.Name)"}}
Invoke-SlormChecked 'node' @("$unpacked\source\tools\verify-source.js")
$fixture=Join-Path $stage 'original';New-Item -ItemType Directory -Path $fixture|Out-Null
[IO.File]::WriteAllText("$fixture\fixture.cpp",'#include <windows.h>'+"`n"+'int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){return 0;}')
Invoke-SlormChecked $tc.cl @('/nologo','/EHsc','/std:c++20','/MD','/O2',"/Fe:$fixture\The Slormancer.exe","/Fo:$fixture\fixture.obj","$fixture\fixture.cpp",'/link','/SUBSYSTEM:WINDOWS','user32.lib')
Invoke-SlormChecked "$unpacked\SlormLoaderSetup.exe" @('self-test',$fixture,"$stage\transactions",'--quiet')
$core="$unpacked\plugins\AurieCore.asi";$bytes=[IO.File]::ReadAllBytes($core);$originalByte=$bytes[0]
try{
    $bytes[0]=$bytes[0] -bxor 1;[IO.File]::WriteAllBytes($core,$bytes)
    $start=[Diagnostics.ProcessStartInfo]::new("$unpacked\SlormLoaderSetup.exe",'self-test "'+$fixture+'" "'+$stage+'\tampered-transaction" --quiet')
    $start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
    $process=[Diagnostics.Process]::Start($start)
    try{$stdout=$process.StandardOutput.ReadToEnd();$stderr=$process.StandardError.ReadToEnd();$process.WaitForExit();$exitCode=$process.ExitCode}finally{$process.Dispose()}
    if($exitCode -eq 0 -or $stderr -notmatch 'Loader package is missing or changed'){throw 'Corrupted packaged Core was not rejected by setup.'}
    if((Get-FileHash "$stage\tampered-transaction\game\The Slormancer.exe").Hash -ne (Get-FileHash "$fixture\The Slormancer.exe").Hash){throw 'Setup changed the fixture EXE before rejecting corrupted package content.'}
}finally{$bytes[0]=$originalByte;[IO.File]::WriteAllBytes($core,$bytes)}
[ordered]@{archiveSha256=(Get-FileHash $ArchivePath).Hash.ToLowerInvariant();sourceFilesChecked=@($sourcePins.PSObject.Properties).Count;officialYYToolkit=$true;packageInstallRepeatRestore=$true;tamperedCoreRejected=$true;gameLaunched=$false;result='passed'}|ConvertTo-Json|Set-Content -LiteralPath "$root\build\package-results.json" -Encoding utf8
Write-Output 'PASS actual ZIP contents, source checksums, package integrity and proxy setup/removal preserve a benign PE byte for byte. No game launched.'
