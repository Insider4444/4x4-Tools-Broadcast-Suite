[CmdletBinding()]
param([string]$AdobeSdk,[string]$ArSdk,[string]$VfxSdk,[string]$AfxSdk)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'sdk-paths.ps1')
$paths=Get-4x4SdkPaths -Adobe $AdobeSdk -Ar $ArSdk -Vfx $VfxSdk -Afx $AfxSdk
foreach($name in @('adobe','ar','vfx','afx')) {
    if(!(Test-Path -LiteralPath $paths[$name] -PathType Container)) {throw "Missing $name SDK folder. See SDK-SETUP.md."}
    $paths[$name]=(Resolve-Path -LiteralPath $paths[$name]).Path
}
$file=Join-Path $PSScriptRoot '..\sdk-paths.local.json'
[IO.File]::WriteAllText([IO.Path]::GetFullPath($file),($paths | ConvertTo-Json),[Text.UTF8Encoding]::new($false))
Write-Host 'Local SDK paths saved in git-ignored sdk-paths.local.json. No credentials were saved.'
