[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$NsisCompiler)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if (!(Test-Path -LiteralPath $NsisCompiler)) { throw 'NSIS compiler missing. Pass -NsisCompiler with a local NSIS 3 path.' }
& python (Join-Path $PSScriptRoot 'package_runtime.py')
if ($LASTEXITCODE -ne 0) { throw 'Runtime staging failed.' }
New-Item -ItemType Directory -Path (Join-Path $root 'dist') -Force | Out-Null
$stage = Join-Path $root 'build\package-stage'
$output = Join-Path $root 'dist\4x4EyeContact-0.1.0-dev.1-Setup.exe'
& $NsisCompiler /V2 /INPUTCHARSET UTF8 "/DPROJECT_ROOT=$root" "/DSTAGE_DIR=$stage" "/DOUTPUT_FILE=$output" (Join-Path $root 'installer\EyeContact.nsi')
if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed.' }
$hash = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText("$output.sha256", "$hash  $([IO.Path]::GetFileName($output))`n")
Write-Host "Installer created: $output"
