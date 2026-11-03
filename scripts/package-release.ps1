[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$NsisCompiler)
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if(!(Test-Path -LiteralPath $NsisCompiler)) {throw 'Pass the official local NSIS 3 compiler.'}
& python (Join-Path $PSScriptRoot 'package_release.py')
if($LASTEXITCODE -ne 0) {throw 'Encrypted release packaging failed.'}
$output=Join-Path $root 'dist\release\4x4Tools-Setup.exe'
& $NsisCompiler /V2 /INPUTCHARSET UTF8 /DWEB_RELEASE "/DPROJECT_ROOT=$root" "/DOUTPUT_FILE=$output" (Join-Path $root 'installer\BroadcastSuite.nsi')
if($LASTEXITCODE -ne 0) {throw 'Release Setup compilation failed.'}
$files=@(Get-ChildItem -LiteralPath (Split-Path -Parent $output) -File | Where-Object {$_.Name -ne 'SHA256SUMS.txt'})
$hashes=@($files | Sort-Object Name | ForEach-Object { (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()+'  '+$_.Name })
[IO.File]::WriteAllLines((Join-Path $root 'dist\release\SHA256SUMS.txt'),$hashes,[Text.UTF8Encoding]::new($false))
Write-Host 'Encrypted end-user release assets are ready in dist/release. Audit before publication.'
