# Keep native process startup separate from the full installer script. NSIS
# extracts native DLLs into its helper directory; they are not .NET assemblies.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$PackageRoot,
    [Parameter(Mandatory=$true)][string]$Manifest,
    [Parameter(Mandatory=$true)][string]$Architecture,
    [string]$InstallRoot,
    [string]$Components='all',
    [switch]$ValidateOnly,
    [switch]$StageOnly,
    [string]$StageRoot,
    [string]$ArSdkRoot,
    [string]$VfxSdkRoot,
    [string]$AfxSdkRoot,
    [string]$KeyFile,
    [string]$PayloadCache,
    [string]$LogFile,
    [long]$ProgressWindow=0
)
$ErrorActionPreference='Stop'
try {
    Set-Location -LiteralPath $PSHOME
    [Environment]::CurrentDirectory=$PSHOME
    # Load the framework from its installed location before entering the helper.
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    & (Join-Path $PSScriptRoot 'install-suite.ps1') @PSBoundParameters
} catch {
    Write-Output ("Setup could not start or finish: "+$_.Exception.Message)
    Write-Output $_.InvocationInfo.PositionMessage
    if($LogFile) { Write-Output "Setup log: $LogFile" }
    exit 1
}
