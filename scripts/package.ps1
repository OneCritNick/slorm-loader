param([string]$YYToolkitPath)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$build=Join-Path $root 'build'
$meta=Get-Content -LiteralPath "$root\src\slorm-runtime.json" -Raw|ConvertFrom-Json
if($meta.version -notmatch '^\d+\.\d+\.\d+$'){throw 'Invalid runtime package version'}
$provenance=Get-Content -LiteralPath "$build\build-provenance.json" -Raw|ConvertFrom-Json
foreach($name in @('SlormLoader.dll','SlormLoaderSetup.exe','AurieCore.dll','AuriePatcher.exe')){
    $pin=$provenance.hashes.$name
    if(-not $pin -or (Get-FileHash -LiteralPath "$build\$name").Hash.ToLowerInvariant() -ne $pin){throw "Build output missing or changed: $name. Run build.ps1 first."}
}
$sdk=Get-Content -LiteralPath "$root\third_party\yytk-sdk\provenance.json" -Raw|ConvertFrom-Json
$expected=$sdk.files.'YYToolkit.dll'
$url='https://github.com/AurieFramework/YYToolkit/releases/download/v5.0.0c/YYToolkit.dll'
if(-not $YYToolkitPath){
    $dependencies=Join-Path $build 'dependencies';New-Item -ItemType Directory -Path $dependencies -Force|Out-Null
    $YYToolkitPath=Join-Path $dependencies 'YYToolkit-5.0.0c.dll'
    if(-not(Test-Path -LiteralPath $YYToolkitPath)){
        [Net.ServicePointManager]::SecurityProtocol=[Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
        $download=Join-Path $dependencies ([Guid]::NewGuid().ToString('N')+'.download')
        Invoke-WebRequest -Uri $url -OutFile $download -UseBasicParsing
        if((Get-FileHash -LiteralPath $download).Hash.ToLowerInvariant() -ne $expected){throw 'Official YYToolkit download checksum differs; no package created.'}
        Move-Item -LiteralPath $download -Destination $YYToolkitPath
    }
}
if((Get-FileHash -LiteralPath $YYToolkitPath).Hash.ToLowerInvariant() -ne $expected){throw 'YYToolkit must match the pinned official 5.0.0c release. Source candidates are not accepted for packaging.'}
$packages=Join-Path $build 'packages';New-Item -ItemType Directory -Path $packages -Force|Out-Null
$stage=Join-Path $packages ([Guid]::NewGuid().ToString('N'));$payload=Join-Path $stage 'runtime'
New-Item -ItemType Directory -Path "$payload\mods\Native","$payload\mods\Aurie","$payload\mods\Slormancer","$payload\tools","$payload\docs","$payload\source"|Out-Null
Copy-Item -LiteralPath "$build\SlormLoader.dll" -Destination "$payload\mods\Aurie\SlormLoader.dll"
Copy-Item -LiteralPath "$build\AurieCore.dll" -Destination "$payload\mods\Native\AurieCore.dll"
Copy-Item -LiteralPath "$build\AuriePatcher.exe" -Destination "$payload\tools\AuriePatcher.exe"
Copy-Item -LiteralPath "$build\SlormLoaderSetup.exe" -Destination "$payload\SlormLoaderSetup.exe"
Copy-Item -LiteralPath $YYToolkitPath -Destination "$payload\mods\Aurie\YYToolkit.dll"
Copy-Item -LiteralPath "$root\src\slorm-runtime.json" -Destination "$payload\slorm-runtime.json"
Copy-Item -LiteralPath "$root\docs\INSTALL.md" -Destination "$payload\docs\README.md"
Copy-Item -LiteralPath "$root\LICENSE" -Destination "$payload\docs\LICENSE-AGPL.txt"
Copy-Item -LiteralPath "$root\licenses\PROJECT-MIT.txt" -Destination "$payload\docs\LICENSE-setup-MIT.txt"
Copy-Item -LiteralPath "$root\src\vendor\nlohmann\LICENSE.MIT" -Destination "$payload\docs\LICENSE-json-MIT.txt"
Copy-Item -LiteralPath "$root\THIRD_PARTY_NOTICES.md" -Destination "$payload\docs\CREDITS.md"
$notices='Third-party notices for the runtime dependencies.'+[Environment]::NewLine
foreach($name in @('spdlog-LICENSE.txt','fmt-LICENSE.txt','SafetyHook-LICENSE.txt','Zydis-Zycore-notices.txt')){$notices+=[Environment]::NewLine+$name+[Environment]::NewLine+(Get-Content -LiteralPath "$root\licenses\$name" -Raw)}
[IO.File]::WriteAllText("$payload\docs\THIRD-PARTY-NOTICES.txt",$notices)
[IO.File]::WriteAllText("$payload\mods\Slormancer\README.txt",'Manual JSON mods: create <mod-id>\mod.json here. Close the game before deployment changes. Vortex manages only its deployed mods.')
# Explicit source allowlist: never copy a game, build directory, toolchain, Git credentials or sibling projects.
foreach($dir in @('src','scripts','tests','third_party','tools','docs','licenses')){
    $entries=@(Get-ChildItem -LiteralPath "$root\$dir" -Force -Recurse)
    if(@($entries|Where-Object {($_.Attributes -band [IO.FileAttributes]::ReparsePoint) -or ($_.PSIsContainer -and $_.Name -in @('.git','node_modules','build'))}).Count){throw "Linked/generated directories cannot enter source: $dir"}
    Copy-Item -LiteralPath "$root\$dir" -Destination "$payload\source\$dir" -Recurse
}
foreach($name in @('build.ps1','README.md','LICENSE','SECURITY.md','THIRD_PARTY_NOTICES.md','.gitattributes','.gitignore')){Copy-Item -LiteralPath "$root\$name" -Destination "$payload\source\$name"}
$sourceHashes=[ordered]@{}
Get-ChildItem -LiteralPath "$payload\source" -File -Recurse|Sort-Object FullName|ForEach-Object{
    if($_.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Linked source files cannot enter a package'}
    if($_.Extension -match '^\.(exe|dll|pdb|obj|lib|zip|rar|log)$'){throw "Unexpected generated file in source: $($_.Name)"}
    $relative=$_.FullName.Substring(("$payload\source").Length+1).Replace('\','/')
    $sourceHashes[$relative]=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()
}
$sourceHashes|ConvertTo-Json -Depth 4|Set-Content -LiteralPath "$payload\docs\source-checksums.json" -Encoding utf8
$pins=[ordered]@{}
foreach($name in @('mods\Native\AurieCore.dll','mods\Aurie\YYToolkit.dll','mods\Aurie\SlormLoader.dll','tools\AuriePatcher.exe','slorm-runtime.json')){$pins[$name]=(Get-FileHash -LiteralPath "$payload\$name").Hash.ToLowerInvariant()}
$pins|ConvertTo-Json|Set-Content -LiteralPath "$payload\slorm-loader-checksums.json" -Encoding utf8
[ordered]@{schemaVersion=1;runtimeVersion=$meta.version;kind='development-source-build';builtAtUtc=[DateTime]::UtcNow.ToString('o');toolchain=$provenance;yyToolkit=@{release='v5.0.0c';url=$url;sha256=$expected;origin='Pinned official release binary; not the optional source candidate'};sourceFileCount=$sourceHashes.Count;liveGameTested=$false}|ConvertTo-Json -Depth 8|Set-Content -LiteralPath "$payload\docs\build-info.json" -Encoding utf8
$archive=Join-Path $stage ("Slorm-Loader-"+$meta.version+'.zip')
Compress-Archive -Path "$payload\*" -DestinationPath $archive
$sha=(Get-FileHash -LiteralPath $archive).Hash.ToLowerInvariant()
[IO.File]::WriteAllText((Join-Path $stage 'SHA256SUMS.txt'),$sha+'  '+[IO.Path]::GetFileName($archive)+[Environment]::NewLine)
[ordered]@{archive=$archive;directory=$payload;sha256=$sha;version=$meta.version;developmentBuild=$true}|ConvertTo-Json|Set-Content -LiteralPath "$build\latest-package.json" -Encoding utf8
Write-Output "Built runtime-only archive: $archive"
