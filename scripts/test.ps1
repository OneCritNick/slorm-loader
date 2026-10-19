param([string]$VCToolsPath,[string]$WindowsSdkRoot,[string]$SdkVersion='10.0.22621.0',[string]$FrameworkArchive,[string]$PythonPath='python')
$ErrorActionPreference='Stop';$root=Split-Path -Parent $PSScriptRoot
if($FrameworkArchive){$FrameworkArchive=(Resolve-Path -LiteralPath $FrameworkArchive).Path}
. (Join-Path $PSScriptRoot 'toolchain.ps1');$tc=Initialize-SlormToolchain $VCToolsPath $WindowsSdkRoot $SdkVersion
$output=Join-Path $root 'build';New-Item -ItemType Directory -Force -Path $output|Out-Null
$stage=Join-Path $output ('tests-'+[Guid]::NewGuid().ToString('N'));New-Item -ItemType Directory -Path $stage|Out-Null
Push-Location $root
try{
    Invoke-SlormChecked $tc.cl @('/nologo','/EHsc','/std:c++20','/MD','/O2','/utf-8',"/Fe:$output\loader-status-tests.exe","/Fo:$stage\loader-status-tests.obj","$root\tests\loader-status.test.cpp")
    & "$output\loader-status-tests.exe" | Tee-Object -FilePath "$output\loader-status-results.txt"
    if($LASTEXITCODE -ne 0){throw 'Loader status tests failed.'}
    Invoke-SlormChecked 'node' @("$root\tests\loader-boundary.test.cjs")
    Invoke-SlormChecked 'node' @("$root\tests\source-verification.test.cjs")
    Invoke-SlormChecked $PythonPath @("$root/tests/loader-load.test.py","$root/build/SlormLoader.dll")
    foreach($name in @('AurieCore.dll','AurieCore-original.dll')){if(-not(Test-Path -LiteralPath "$output\$name")){throw 'Run scripts/build.ps1 before the complete offline test suite.'}}
    if($FrameworkArchive){
        $pin=Get-Content "$root/third_party/runtime-framework/provenance.json" -Raw|ConvertFrom-Json
        if((Get-FileHash -LiteralPath $FrameworkArchive).Hash.ToLowerInvariant() -ne $pin.archiveSha256){throw 'Framework dependency archive differs'}
        Write-Output 'PASS exact previously validated framework archive; Core/proxy startup suites are not repeated for unchanged shipped dependencies.'
    }else{
        & "$PSScriptRoot\test-core.ps1" -Toolchain $tc -Root $root
        & "$PSScriptRoot\test-proxy.ps1" -Toolchain $tc -Root $root
    }
    Invoke-SlormChecked 'node' @("$root\tests\bootstrap.test.cjs")
    Write-Output 'PASS offline native and EXE-free installer checks; no game or saves used.'
}finally{Pop-Location}
