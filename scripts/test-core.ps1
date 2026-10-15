param($Toolchain,[string]$Root)
$ErrorActionPreference='Stop';$tc=$Toolchain;$stage=Join-Path $Root ('build\core-tests-'+[Guid]::NewGuid().ToString('N'));New-Item -ItemType Directory -Path $stage|Out-Null
Push-Location $stage
try{
    Invoke-SlormChecked $tc.cl @('/nologo','/EHsc','/std:c++20','/MD','/O2','/DNDEBUG',"/Fe:$stage\aurie-core-load.test.exe","$Root\tests\aurie-core-load.test.cpp",'/link','/SUBSYSTEM:WINDOWS','user32.lib')
    $negative=Join-Path $stage 'negative';New-Item -ItemType Directory -Path $negative|Out-Null
    Copy-Item -LiteralPath "$stage\aurie-core-load.test.exe" -Destination "$negative\aurie-core-load.test.exe";Copy-Item -LiteralPath "$Root\build\AurieCore.dll" -Destination "$negative\Core-under-test.dll"
    $strip=Start-Process -FilePath "$negative\aurie-core-load.test.exe" -ArgumentList '--remove-manifest' -WorkingDirectory $negative -WindowStyle Hidden -PassThru
    if(-not $strip.WaitForExit(30000)){$strip.Kill();throw 'Negative manifest fixture exceeded 30 seconds.'};if($strip.ExitCode -ne 0){throw 'Negative manifest fixture failed.'}
    $cases=[ordered]@{original="$Root\build\AurieCore-original.dll";'missing-manifest'="$negative\Core-under-test.dll"}
    for($i=1;$i -le 5;$i++){$cases["candidate-$i"]="$Root\build\AurieCore.dll"}
    $results=@()
    foreach($case in $cases.GetEnumerator()){
        $fixture=Join-Path $stage $case.Key;New-Item -ItemType Directory -Path "$fixture\mods\Native","$fixture\mods\Aurie"|Out-Null
        Copy-Item -LiteralPath "$stage\aurie-core-load.test.exe" -Destination "$fixture\aurie-core-load.test.exe";Copy-Item -LiteralPath $case.Value -Destination "$fixture\Core-under-test.dll"
        $process=Start-Process -FilePath "$fixture\aurie-core-load.test.exe" -WorkingDirectory $fixture -WindowStyle Hidden -PassThru
        # Allow OS process startup/console diagnostics time. The native host still
        # independently fails a LoadLibrary call that exceeds 3.5 seconds.
        if(-not $process.WaitForExit(30000)){$process.Kill();throw "Core fixture timed out before completing diagnostics: $($case.Key)"}
        if($process.ExitCode -ne 0){throw "Core fixture failed: $($case.Key)"}
        $report=Get-Content -LiteralPath "$fixture\core-load-result.json" -Raw|ConvertFrom-Json
        $report|Add-Member -NotePropertyName case -NotePropertyValue $case.Key
        $report|Add-Member -NotePropertyName sha256 -NotePropertyValue ((Get-FileHash -LiteralPath "$fixture\Core-under-test.dll").Hash.ToLowerInvariant())
        $report|Add-Member -NotePropertyName ownedConsoleBanner -NotePropertyValue (([string]$report.consoleText).Contains('Aurie console logger uses an independent output handle'))
        $results+=$report
        Write-Output "$($case.Key): loaded=$($report.loadSuccess), error=$($report.loadErrorCode), startup=$($report.coreStartupHeader), ownedConsole=$($report.ownedConsoleBanner)"
    }
    $results|ConvertTo-Json -Depth 8|Set-Content -LiteralPath "$Root\build\core-load-results.json" -Encoding utf8
    if(-not $results[0].loadSuccess -or -not $results[0].coreStartupHeader){throw 'Source-built original Core did not reach logger startup.'}
    $bad=$results|Where-Object case -eq 'missing-manifest';if($bad.loadSuccess -or $bad.loadErrorCode -ne 182 -or $bad.coreLogBytes -ne 0 -or $bad.manifestResource2Present -or $bad.commonControlsOrdinal345Available){throw 'Negative missing-manifest fixture failed to reproduce ordinal182.'}
    $good=@($results|Where-Object {$_.case.StartsWith('candidate')});if($good.Count -ne 5 -or @($good|Where-Object {-not $_.loadSuccess -or -not $_.coreStartupHeader -or -not $_.manifestCommonControls6 -or -not $_.commonControlsOrdinal345Available -or -not $_.ownedConsoleBanner}).Count){throw 'Candidate startup/console/manifest gate failed.'}
    if(@($good.sha256|Select-Object -Unique).Count -ne 1){throw 'Candidate changed across repetitions.'}
    if($results|Where-Object {$_.consoleWindowVisible -or $_.consoleVisibleObserved}){throw 'A fixture console became visible.'}
    Write-Output 'PASS source-built original and five candidate Core startups, negative manifest regression; no game/mod DLL loaded.'
}finally{Pop-Location}
