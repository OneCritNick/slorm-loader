param([string]$VCToolsPath,[string]$WindowsSdkRoot,[string]$SdkVersion='10.0.22621.0',[string]$YYToolkitPath,[string]$FrameworkArchive,[string]$PythonPath='python')
$ErrorActionPreference='Stop'
$toolchain=@{SdkVersion=$SdkVersion}
if($VCToolsPath){$toolchain.VCToolsPath=$VCToolsPath}
if($WindowsSdkRoot){$toolchain.WindowsSdkRoot=$WindowsSdkRoot}
& "$PSScriptRoot\scripts\build.ps1" @toolchain
& "$PSScriptRoot\scripts\test.ps1" @toolchain -FrameworkArchive $FrameworkArchive -PythonPath $PythonPath
& "$PSScriptRoot\scripts\package.ps1" -YYToolkitPath $YYToolkitPath -FrameworkArchive $FrameworkArchive
$package=Get-Content -LiteralPath "$PSScriptRoot\build\latest-package.json" -Raw|ConvertFrom-Json
& "$PSScriptRoot\scripts\test-package.ps1" @toolchain -ArchivePath $package.archive -NexusArchivePath $package.nexusArchive
Write-Output "Complete tested source bundle: $($package.archive)"
Write-Output "Tested Nexus player bundle: $($package.nexusArchive)"
