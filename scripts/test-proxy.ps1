param($Toolchain,[string]$Root)
$ErrorActionPreference='Stop';$tc=$Toolchain
. "$PSScriptRoot/proxy-dependency.ps1"
$stage=Join-Path $Root ('build/proxy-tests-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path "$stage/probe/plugins","$stage/native/plugins","$stage/priority/plugins","$stage/priority/mods/Aurie","$stage/running/plugins","$stage/running/mods/Aurie","$stage/missing","$stage/cwd"|Out-Null
$proxy=Get-SlormProxy $Root
Push-Location $stage
try {
    Invoke-SlormChecked $tc.cl @('/nologo','/EHsc','/std:c++20','/MD','/O2','/Zi',"/Fe:$stage/VersionForwardingTest.exe","$Root/tests/version-proxy-host.cpp",'/link','/DEBUG:FULL',"$($tc.sdk)/Lib/$($tc.sdkVersion)/um/x64/version.lib")
    Invoke-SlormChecked $tc.cl @('/nologo','/EHsc','/std:c++20','/MD','/O2','/Zi','/LD',"/Fe:$stage/probe/plugins/AurieCore.asi","$Root/tests/version-proxy-core.cpp",'/link','/DEBUG:FULL')
    Copy-Item -LiteralPath "$Root/build/AurieCore.dll" -Destination "$stage/native/plugins/AurieCore.asi"
    Invoke-SlormChecked $tc.cl @('/nologo','/EHsc','/std:c++20','/MD','/O2','/Zi','/DNDEBUG','/LD',"/I$Root/third_party/yytk-sdk/include","/Fe:$stage/priority/mods/Aurie/PriorityProbe.dll","$Root/tests/priority-module.cpp",'/link','/DEBUG:FULL','user32.lib')
    Invoke-SlormChecked $tc.cl @('/nologo','/EHsc','/std:c++20','/MD','/O2','/Zi','/DNDEBUG','/DNESTED_PROBE','/LD',"/I$Root/third_party/yytk-sdk/include","/Fe:$stage/priority/mods/Aurie/NestedProbe.bin","$Root/tests/priority-module.cpp",'/link','/DEBUG:FULL','user32.lib')
    foreach($folder in @('priority','running')){
        Copy-Item -LiteralPath "$Root/build/AurieCore.dll" -Destination "$stage/$folder/plugins/AurieCore.asi"
        if($folder -eq 'running'){Copy-Item -LiteralPath "$stage/priority/mods/Aurie/PriorityProbe.dll","$stage/priority/mods/Aurie/NestedProbe.bin" -Destination "$stage/running/mods/Aurie"}
    }
    $results=@()
    foreach($case in @(@{folder='probe';mode='ual'},@{folder='native';mode='ual_native'},@{folder='priority';mode='ual_priority'},@{folder='running';mode='ual_running'},@{folder='missing';mode='ual_missing'})){
        $fixture=Join-Path $stage $case.folder
        Copy-Item -LiteralPath "$stage/VersionForwardingTest.exe" -Destination "$fixture/VersionForwardingTest.exe"
        Copy-Item -LiteralPath $proxy -Destination "$fixture/version.dll"
        Copy-Item -LiteralPath "$Root/src/version.ini" -Destination "$fixture/version.ini"
        if($case.mode -eq 'ual_running'){(Get-Content "$fixture/version.ini" -Raw).Replace('LoadPlugins=1','LoadPlugins=0')|Set-Content "$fixture/version.ini"}
        $before=(Get-FileHash "$fixture/VersionForwardingTest.exe").Hash
        $process=Start-Process -FilePath "$fixture/VersionForwardingTest.exe" -ArgumentList $case.mode -WorkingDirectory "$stage/cwd" -WindowStyle Hidden -PassThru -RedirectStandardOutput "$fixture/stdout.txt" -RedirectStandardError "$fixture/stderr.txt"
        if(-not $process.WaitForExit(20000)){$process.Kill();throw "Proxy fixture timed out: $($case.mode)"}
        if($process.ExitCode -ne 0){throw "Proxy fixture failed or Windows blocked it: $($case.mode), exit $($process.ExitCode). Check Code Integrity events and $fixture/stderr.txt"}
        if((Get-FileHash "$fixture/VersionForwardingTest.exe").Hash -ne $before){throw 'Proxy modified the surrogate EXE'}
        if($case.mode -eq 'ual_native' -and -not(Test-Path "$fixture/native-proxy-result.txt")){throw 'Actual Core did not complete initialization'}
        if($case.mode -in @('ual_priority','ual_running') -and (Get-Content "$fixture/priority-phases.txt" -Raw).Replace("`r",'') -ne "entrypoint`npreinitialize`nnested-preinitialize`ninitialize`nnested-initialize`n"){throw 'Startup callback order regression'}
        $results+=@{mode=$case.mode;exitCode=$process.ExitCode;exeUnchanged=$true;gameLaunched=$false}
        Write-Output "PASS proxy fixture $($case.mode): Windows API forwarding, startup and unchanged EXE"
    }
    $results|ConvertTo-Json -Depth 5|Set-Content "$Root/build/proxy-test-results.json"
}finally{Pop-Location}
