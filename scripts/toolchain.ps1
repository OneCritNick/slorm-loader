function Initialize-SlormToolchain {
    param([string]$VCToolsPath,[string]$WindowsSdkRoot,[string]$SdkVersion='10.0.22621.0')
    $ErrorActionPreference='Stop'
    if(-not $VCToolsPath){
        $vswhere=Join-Path ([Environment]::GetEnvironmentVariable('ProgramFiles(x86)')) 'Microsoft Visual Studio\Installer\vswhere.exe'
        if(-not(Test-Path -LiteralPath $vswhere)){throw 'Visual Studio detection failed. Supply -VCToolsPath explicitly.'}
        $vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if($LASTEXITCODE -ne 0 -or -not $vs){throw 'Install Visual Studio2022 C++ build tools or supply -VCToolsPath.'}
        $VCToolsPath=(Get-ChildItem -LiteralPath (Join-Path $vs 'VC\Tools\MSVC') -Directory|Where-Object {Test-Path -LiteralPath (Join-Path $_.FullName 'bin\Hostx64\x64\cl.exe')}|Sort-Object {[version]$_.Name} -Descending|Select-Object -First 1).FullName
    }
    if(-not $WindowsSdkRoot){$WindowsSdkRoot=Join-Path ([Environment]::GetEnvironmentVariable('ProgramFiles(x86)')) 'Windows Kits\10'}
    $vc=(Resolve-Path -LiteralPath $VCToolsPath).Path;$sdk=(Resolve-Path -LiteralPath $WindowsSdkRoot).Path
    foreach($file in @("$vc\bin\Hostx64\x64\cl.exe","$vc\bin\Hostx64\x64\dumpbin.exe","$vc\include\vector","$vc\lib\x64\msvcprt.lib","$sdk\bin\$SdkVersion\x64\rc.exe","$sdk\bin\$SdkVersion\x64\mt.exe","$sdk\Lib\$SdkVersion\um\x64\kernel32.lib")){
        if(-not(Test-Path -LiteralPath $file -PathType Leaf)){throw "Missing compiler/SDK prerequisite: $file"}
    }
    $env:PATH="$vc\bin\Hostx64\x64;$sdk\bin\$SdkVersion\x64;$env:PATH"
    $env:INCLUDE="$vc\include;$sdk\Include\$SdkVersion\ucrt;$sdk\Include\$SdkVersion\shared;$sdk\Include\$SdkVersion\um"
    $env:LIB="$vc\lib\x64;$sdk\Lib\$SdkVersion\ucrt\x64;$sdk\Lib\$SdkVersion\um\x64"
    return [pscustomobject]@{vc=$vc;sdk=$sdk;sdkVersion=$SdkVersion;cl="$vc\bin\Hostx64\x64\cl.exe";dumpbin="$vc\bin\Hostx64\x64\dumpbin.exe";rc="$sdk\bin\$SdkVersion\x64\rc.exe";mt="$sdk\bin\$SdkVersion\x64\mt.exe";toolset=(Split-Path -Leaf $vc);compilerVersion=(Get-Item -LiteralPath "$vc\bin\Hostx64\x64\cl.exe").VersionInfo.FileVersion}
}
function Invoke-SlormChecked {
    param([string]$Tool,[string[]]$Arguments)
    & $Tool @Arguments
    if($LASTEXITCODE -ne 0){throw "$([IO.Path]::GetFileName($Tool)) failed with exit code $LASTEXITCODE"}
}
function Get-SlormExports {
    param($Toolchain,[string]$Dll)
    $lines=& $Toolchain.dumpbin /exports $Dll
    if($LASTEXITCODE -ne 0){throw "Unable to inspect exports: $Dll"}
    return @($lines|ForEach-Object {if($_ -match '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(\S+)'){$matches[1]}}|Sort-Object)
}
function Assert-SlormManifest {
    param($Toolchain,[string]$Binary,[int]$ResourceId,[string]$Output)
    Invoke-SlormChecked $Toolchain.mt @('-nologo',"-inputresource:$Binary;#$ResourceId","-out:$Output")
    $manifest=Get-Content -LiteralPath $Output -Raw
    if($manifest -notmatch 'Microsoft.Windows.Common-Controls' -or $manifest -notmatch 'version="6.0.0.0"'){throw "Common Controls6 manifest missing: $Binary"}
}
