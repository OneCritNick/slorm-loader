function Get-SlormProxy([string]$Root) {
    $pin=Get-Content -LiteralPath "$Root/third_party/ual/provenance.json" -Raw|ConvertFrom-Json
    $dir=Join-Path $Root 'build/dependencies/ual-9.0.0';$archive=Join-Path $dir 'Ultimate-ASI-Loader_x64.zip'
    New-Item -ItemType Directory -Force -Path $dir|Out-Null
    if(-not(Test-Path -LiteralPath $archive)){Invoke-WebRequest -Uri $pin.url -OutFile $archive -UseBasicParsing}
    if((Get-FileHash -LiteralPath $archive).Hash.ToLowerInvariant() -ne $pin.archiveSha256){throw 'Pinned ASI loader archive checksum differs'}
    $dll=Join-Path $dir 'dinput8.dll'
    if(-not(Test-Path -LiteralPath $dll)){Expand-Archive -LiteralPath $archive -DestinationPath $dir}
    if((Get-FileHash -LiteralPath $dll).Hash.ToLowerInvariant() -ne $pin.dllSha256){throw 'Pinned ASI loader DLL checksum differs'}
    return $dll
}
