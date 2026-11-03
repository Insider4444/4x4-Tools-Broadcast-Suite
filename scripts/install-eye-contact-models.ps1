[CmdletBinding()]
param(
    [string]$SdkRoot,
    [ValidateSet('75','86','89','120')][string]$Gpu = '120'
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'sdk-paths.ps1')
if(!$SdkRoot) {$SdkRoot=(Get-4x4SdkPaths).ar}
$SdkRoot = (Resolve-Path -LiteralPath $SdkRoot).Path
$vendorScript = Join-Path $SdkRoot 'features\install_feature.ps1'
if (!(Test-Path -LiteralPath $vendorScript)) { throw "Missing NVIDIA installer: $vendorScript" }
$previousKey = $env:NGC_CLI_API_KEY
$keyPointer = [IntPtr]::Zero
$secureKey = $null
try {
    $dotenv = Join-Path $PSScriptRoot '..\.env'
    if (!$env:NGC_CLI_API_KEY -and (Test-Path -LiteralPath $dotenv)) {
        foreach ($line in [IO.File]::ReadAllLines((Resolve-Path -LiteralPath $dotenv).Path)) {
            if ($line -match '^\s*NGC_CLI_API_KEY\s*=\s*(.*?)\s*$') {
                $env:NGC_CLI_API_KEY = $Matches[1].Trim().Trim('"').Trim("'")
            }
        }
    }
    if (!$env:NGC_CLI_API_KEY -and !(Test-Path -LiteralPath (Join-Path $env:USERPROFILE '.ngc\config'))) {
        Write-Host 'NVIDIA requires an NGC API key to download Eye Contact models.'
        Write-Host 'Set NGC_CLI_API_KEY in .env or enter it at the hidden prompt.'
        $secureKey = Read-Host 'Paste your NGC API key (hidden input)' -AsSecureString
        $keyPointer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secureKey)
        $env:NGC_CLI_API_KEY = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($keyPointer)
    }
    $shell = (Get-Command pwsh -ErrorAction SilentlyContinue).Source
    if (!$shell) { $shell = (Get-Command powershell.exe).Source }
    # NVIDIA's installer resolves gaze dependencies, including landmarks and face detection.
    Push-Location -LiteralPath (Join-Path $SdkRoot 'features')
    try {
        & $shell -NoProfile -ExecutionPolicy Bypass -File $vendorScript -gpu $Gpu -features nvargazeredirection -version 1.2.0.0
    } finally {
        Pop-Location
    }
    if ($LASTEXITCODE -ne 0) { throw "NVIDIA's feature installer failed (exit $LASTEXITCODE)." }
    $gazeDlls = @(Get-ChildItem -LiteralPath (Join-Path $SdkRoot 'features') -Recurse -File -Filter '*GazeRedirection*.dll')
    $models = @(Get-ChildItem -LiteralPath (Join-Path $SdkRoot 'bin\models') -Recurse -File -ErrorAction SilentlyContinue)
    if (!$gazeDlls.Count -or !$models.Count) { throw 'The feature installer did not produce the required gaze library and models.' }
    Write-Host 'Eye Contact libraries and models are present. Run the project diagnostics next.' -ForegroundColor Green
} finally {
    $env:NGC_CLI_API_KEY = $previousKey
    if ($keyPointer -ne [IntPtr]::Zero) { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($keyPointer) }
    if ($secureKey) { $secureKey.Dispose() }
}
