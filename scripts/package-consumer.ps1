[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$NsisCompiler)
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
& python (Join-Path $PSScriptRoot 'package_consumer.py')
if($LASTEXITCODE -ne 0){throw 'Consumer package preparation failed.'}
$output=Join-Path $root 'dist\community-0.2.0\public\4x4Tools-Setup.exe'
& $NsisCompiler /V2 /INPUTCHARSET UTF8 /DWEB_RELEASE /DSUITE_VERSION=0.2.0 "/DPROJECT_ROOT=$root" "/DOUTPUT_FILE=$output" "/DSUITE_MANIFEST=$root\build\consumer-manifest.json" "/DSUITE_LICENSE=$root\build\consumer-license.txt" (Join-Path $root 'installer\BroadcastSuite.nsi')
if($LASTEXITCODE -ne 0){throw 'Consumer Setup compilation failed.'}
$hash=(Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText((Join-Path $root 'dist\community-0.2.0\public\SHA256SUMS.txt'),"$hash  4x4Tools-Setup.exe`n",[Text.UTF8Encoding]::new($false))
Write-Host 'Public release: Setup + checksum only. The support directory is installer-managed storage, not a user download set.'
