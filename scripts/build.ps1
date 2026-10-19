param([string]$VCToolsPath,[string]$WindowsSdkRoot,[string]$SdkVersion='10.0.22621.0',[switch]$BuildYYToolkit)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'toolchain.ps1')
$tc=Initialize-SlormToolchain $VCToolsPath $WindowsSdkRoot $SdkVersion
$output=Join-Path $root 'build';New-Item -ItemType Directory -Force -Path $output|Out-Null
$stage=Join-Path $output ('compile-'+[Guid]::NewGuid().ToString('N'));New-Item -ItemType Directory -Path $stage|Out-Null
$sdk=Join-Path $root 'third_party\yytk-sdk';$include=Join-Path $sdk 'include'
$pin=Get-Content -LiteralPath (Join-Path $sdk 'provenance.json') -Raw|ConvertFrom-Json
if($pin.sourceCommit -ne 'd5cc0078bfbfd5907884ad3fe0b3c6cbaa1c3f54' -or $pin.release -ne 'v5.0.0c'){throw 'Unexpected YYToolkit SDK pin.'}
foreach($item in $pin.files.PSObject.Properties){
    if($item.Name -eq 'YYToolkit.dll'){continue} # This source build does not require/download an upstream binary.
    $file=Join-Path $sdk $item.Name
    if((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $item.Value){throw "SDK pin differs: $($item.Name)"}
}
Push-Location $stage
try {
    Invoke-SlormChecked $tc.cl @('/nologo','/EHsc','/std:c++20','/MD','/O2','/LD','/DNDEBUG',"/I$include","/Fe:$stage\SlormLoader.dll","$root\src\SlormLoader.cpp","$include\YYToolkit\YYTK_Shared_Types.cpp",'/link','user32.lib')
    $exports=Get-SlormExports $tc "$stage\SlormLoader.dll"
    foreach($name in @('__AurieFrameworkInit','__AurieIsDebugBuild','ModuleInitialize','ModuleUnload')){if($name -notin $exports){throw "Loader export missing: $name"}}
    function Build-Core([string]$Tree,[string]$Name){
        $unit=Join-Path $stage $Name;New-Item -ItemType Directory -Path $unit|Out-Null;Push-Location $unit
        try{
            $source=Join-Path $Tree 'Aurie\source'
            Invoke-SlormChecked $tc.rc @('/nologo','/c65001',"/fo$unit\AurieCore.res","/I$source\..","$source\..\AurieCore.rc")
            Invoke-SlormChecked $tc.cl @('/nologo','/TC','/MD','/O2','/DNDEBUG','/DZYDIS_STATIC_BUILD',"/I$source\include",'/c',"$source\include\Zydis\Zydis.c")
            $cpp=@(Get-ChildItem -LiteralPath $source -Recurse -Filter '*.cpp'|ForEach-Object FullName)
            $compileArgs=@('/nologo','/EHsc','/std:c++latest','/MD','/O2','/LD','/MP2','/utf-8','/DNDEBUG','/DAURIE_INCLUDE_PRIVATE','/DZYDIS_STATIC_BUILD','/DWIN32','/D_CONSOLE','/DUNICODE','/D_UNICODE',"/I$source\include","/Fe:$unit\AurieCore.dll")+$cpp+@("$unit\Zydis.obj","$unit\AurieCore.res",'/link','/SUBSYSTEM:CONSOLE','/MANIFEST:EMBED','ntdll.lib','comctl32.lib','user32.lib')
            Invoke-SlormChecked $tc.cl $compileArgs
            Assert-SlormManifest $tc "$unit\AurieCore.dll" 2 "$unit\embedded.manifest"
            Copy-Item -LiteralPath "$unit\AurieCore.dll" -Destination "$stage\$Name.dll"
        }finally{Pop-Location}
    }
    Build-Core (Join-Path $root 'third_party\aurie') 'AurieCore'
    Build-Core (Join-Path $root 'third_party\aurie-original') 'AurieCore-original'
    $original=Get-SlormExports $tc "$stage\AurieCore-original.dll";$modified=Get-SlormExports $tc "$stage\AurieCore.dll"
    if(-not $original.Count -or (Compare-Object ($original+@('InitializeASI')|Sort-Object) $modified)){throw 'Core exports must retain every upstream export and add only InitializeASI.'}
    $products=@('SlormLoader.dll','AurieCore.dll','AurieCore-original.dll')
    if($BuildYYToolkit){
        $tree=Join-Path $root 'third_party\yytk-source\YYToolkit';$unit=Join-Path $stage 'yytk';New-Item -ItemType Directory -Path $unit|Out-Null;Push-Location $unit
        try{
            Invoke-SlormChecked $tc.rc @('/nologo',"/fo$unit\YYToolkit.res","/I$tree","$tree\YYToolkit.rc")
            Invoke-SlormChecked $tc.cl @('/nologo','/TC','/MD','/O2','/DNDEBUG','/DZYDIS_STATIC_BUILD',"/I$tree\include",'/c',"$tree\include\Zydis\Zydis.c")
            $cpp=@(Get-ChildItem -LiteralPath "$tree\source" -Recurse -Filter '*.cpp'|ForEach-Object FullName)
            Invoke-SlormChecked $tc.cl (@('/nologo','/EHsc','/std:c++latest','/MD','/O2','/LD','/MP2','/DNDEBUG','/DYYTK_INCLUDE_PRIVATE','/DYYTK_DEFINE_INTERNAL','/DZYDIS_STATIC_BUILD','/DUNICODE','/D_UNICODE',"/I$tree\include","/Fe:$stage\YYToolkit-source-candidate.dll")+$cpp+@("$unit\Zydis.obj","$unit\YYToolkit.res",'/link','user32.lib'))
            $products+='YYToolkit-source-candidate.dll'
        }finally{Pop-Location}
    }
    $hashes=[ordered]@{};foreach($name in $products){Copy-Item -LiteralPath "$stage\$name" -Destination "$output\$name" -Force;$hashes[$name]=(Get-FileHash -LiteralPath "$output\$name" -Algorithm SHA256).Hash.ToLowerInvariant()}
    [ordered]@{schemaVersion=1;builtAtUtc=[DateTime]::UtcNow.ToString('o');toolset=$tc.toolset;compilerVersion=$tc.compilerVersion;windowsSdk=$tc.sdkVersion;sdkSourceCommit=$pin.sourceCommit;corePreservesUpstreamExports=$true;coreAdditionalExports=@('InitializeASI');embeddedCommonControls6=$true;hashes=$hashes;scope='Independent source build; not claimed byte-identical to historical release or live-game-tested';yyToolkitSourceCandidate=[bool]$BuildYYToolkit}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath "$output\build-provenance.json" -Encoding utf8
    Write-Output "Built $($products.Count) source outputs in $output with MSVC$($tc.toolset), compiler$($tc.compilerVersion), SDK$($tc.sdkVersion). Run scripts/test.ps1 before using outputs."
}finally{Pop-Location}
