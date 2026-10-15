param([string]$VCToolsPath,[string]$WindowsSdkRoot,[string]$SdkVersion='10.0.22621.0')
$ErrorActionPreference='Stop';$root=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'toolchain.ps1');$tc=Initialize-SlormToolchain $VCToolsPath $WindowsSdkRoot $SdkVersion
$output=Join-Path $root 'build';New-Item -ItemType Directory -Force -Path $output|Out-Null
$stage=Join-Path $output ('tests-'+[Guid]::NewGuid().ToString('N'));New-Item -ItemType Directory -Path $stage|Out-Null
Push-Location $root
try{
    Invoke-SlormChecked 'node' @("$root\tools\generate-field-spec.js")
    Invoke-SlormChecked $tc.cl @('/nologo','/EHsc','/std:c++20','/MD','/O2','/utf-8',"/Fe:$output\stat-api-tests.exe","/Fo:$stage\stat-api-tests.obj","$root\tests\stat-api.test.cpp")
    $native=Join-Path $stage 'native';New-Item -ItemType Directory -Path "$native\src","$native\tests"|Out-Null
    Copy-Item -LiteralPath "$root\tests\fixtures" -Destination "$native\tests\fixtures" -Recurse
    Copy-Item -LiteralPath "$root\src\slorm-runtime.json" -Destination "$native\src\slorm-runtime.json"
    Push-Location $native
    try{& "$output\stat-api-tests.exe"|Tee-Object -FilePath "$output\stat-api-results.txt";if($LASTEXITCODE -ne 0){throw 'Native stat tests failed.'}}finally{Pop-Location}
    foreach($name in @('AurieCore.dll','AurieCore-original.dll','AuriePatcher.exe','SlormLoaderSetup.exe')){if(-not(Test-Path -LiteralPath "$output\$name")){throw 'Run scripts/build.ps1 before the complete offline test suite.'}}
    & "$PSScriptRoot\test-core.ps1" -Toolchain $tc -Root $root
    # Benign own x64 PE is only patched/restored; never run as a game.
    $bootstrap=Join-Path $stage 'bootstrap';$fixture=Join-Path $bootstrap 'source';$package=Join-Path $bootstrap 'package'
    New-Item -ItemType Directory -Path $fixture,"$package\mods\Native","$package\tools"|Out-Null
    [IO.File]::WriteAllText("$fixture\fixture.cpp",'#include <windows.h>'+"`n"+'int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){return 0;}')
    Invoke-SlormChecked $tc.cl @('/nologo','/EHsc','/std:c++20','/MD','/O2',"/Fe:$fixture\The Slormancer.exe","/Fo:$fixture\fixture.obj","$fixture\fixture.cpp",'/link','/SUBSYSTEM:WINDOWS','user32.lib')
    Copy-Item -LiteralPath "$output\SlormLoaderSetup.exe" -Destination "$package\SlormLoaderSetup.exe"
    Copy-Item -LiteralPath "$output\AurieCore.dll" -Destination "$package\mods\Native\AurieCore.dll"
    Copy-Item -LiteralPath "$output\AuriePatcher.exe" -Destination "$package\tools\AuriePatcher.exe"
    $pins=[ordered]@{};foreach($name in @('mods\Native\AurieCore.dll','tools\AuriePatcher.exe')){$pins[$name]=(Get-FileHash -LiteralPath "$package\$name").Hash.ToLowerInvariant()}
    $pins|ConvertTo-Json|Set-Content -LiteralPath "$package\slorm-loader-checksums.json" -Encoding utf8
    Invoke-SlormChecked "$package\SlormLoaderSetup.exe" @('self-test',$fixture,"$bootstrap\transaction",'--quiet')
    Write-Output 'PASS complete offline tests. Fixture EXE was patched/restored, never launched; no game assets, installed game or saves used.'
}finally{Pop-Location}
