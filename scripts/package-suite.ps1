[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$NsisCompiler,
    [switch]$SkipPayloadBuild,
    [switch]$CreateArchive
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if (!(Test-Path -LiteralPath $NsisCompiler)) { throw 'Pass -NsisCompiler with the local NSIS 3 compiler.' }
& python (Join-Path $PSScriptRoot 'make-installer-art.py')
if ($LASTEXITCODE -ne 0) { throw 'Installer artwork compilation failed.' }
$arguments=@((Join-Path $PSScriptRoot 'package_suite.py'))
if ($SkipPayloadBuild) { $arguments+='--manifest-only' }
& python @arguments
if ($LASTEXITCODE -ne 0) { throw 'Suite payload packaging failed.' }
$output=Join-Path $root 'dist\4x4Tools-Broadcast-Suite-0.2.0-preview.1\4x4Tools-Setup.exe'
& $NsisCompiler /V2 /INPUTCHARSET UTF8 "/DPROJECT_ROOT=$root" "/DOUTPUT_FILE=$output" (Join-Path $root 'installer\BroadcastSuite.nsi')
if ($LASTEXITCODE -ne 0) { throw 'Suite installer compilation failed.' }
$folder=Split-Path -Parent $output
$hashes=[Collections.Generic.List[string]]::new()
foreach ($path in @((Get-Item -LiteralPath $output)) + @(Get-ChildItem -LiteralPath (Join-Path $folder 'Data') -File -Filter '*.zip')) {
    $hash=(Get-FileHash -LiteralPath $path.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $relative=$path.FullName.Substring($folder.Length+1).Replace('\','/')
    $hashes.Add("$hash  $relative")
}
[IO.File]::WriteAllLines((Join-Path $folder 'SHA256SUMS.txt'),$hashes,[Text.UTF8Encoding]::new($false))
Write-Host "Suite package ready: $folder"
if ($CreateArchive) {
    & python (Join-Path $PSScriptRoot 'archive-suite.py')
    if ($LASTEXITCODE -ne 0) { throw 'Portable archive creation failed.' }
}
