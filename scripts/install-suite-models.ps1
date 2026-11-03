[CmdletBinding()]
param(
    [ValidateSet('Video','Audio')][string]$Kind = 'Video',
    [ValidateSet('75','86','89','120')][string]$Gpu = '120',
    [switch]$AllArchitectures,
    [switch]$ListOnly,
    [switch]$IncludeSpeakerFocus,
    [string]$SdkRoot
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
. (Join-Path $PSScriptRoot 'sdk-paths.ps1')
$paths=Get-4x4SdkPaths
$previousKey = $env:NGC_CLI_API_KEY
try {
    if (!$env:NGC_CLI_API_KEY -and (Test-Path -LiteralPath (Join-Path $root '.env'))) {
        foreach ($line in [IO.File]::ReadAllLines((Join-Path $root '.env'))) {
            if ($line -match '^\s*NGC_CLI_API_KEY\s*=\s*(.*?)\s*$') {
                $env:NGC_CLI_API_KEY = $Matches[1].Trim().Trim('"').Trim("'")
            }
        }
    }
    if (!$env:NGC_CLI_API_KEY) { throw 'Configure NGC_CLI_API_KEY in .env before downloading models.' }
    $shell = (Get-Command pwsh -ErrorAction SilentlyContinue).Source
    if (!$shell) { $shell = (Get-Command powershell.exe).Source }
    $architectures = if ($AllArchitectures) { @('120','89','86','75') } else { @($Gpu) }
    if ($Kind -eq 'Video') {
        $selected=if($SdkRoot){$SdkRoot}else{$paths.vfx}
        $features = Join-Path $selected 'features'
        $vendor = Join-Path $features 'install_feature.ps1'
        $effects = 'nvvfxgreenscreen,nvvfxbackgroundblur,nvvfxdenoising,nvvfxrelighting,nvvfxupscale,nvvfxvideosuperres'
    } else {
        $selected=if($SdkRoot){$SdkRoot}else{$paths.afx}
        $features = Join-Path $selected 'features'
        $vendor = Join-Path $features 'download_features.ps1'
        $effects = 'denoiser-48k,dereverb-48k,dereverb_denoiser-48k,studio_voice_high_quality-48k,studio_voice_low_latency-48k,superres-16k_to_48k'
        if ($IncludeSpeakerFocus) { $effects += ',speaker_focus-48k' }
    }
    Push-Location -LiteralPath $features
    try {
        if ($ListOnly) {
            & $shell -NoProfile -ExecutionPolicy Bypass -File $vendor -list_features
            if ($LASTEXITCODE -ne 0) { throw "Feature discovery failed ($LASTEXITCODE)." }
        } else {
            foreach ($architecture in $architectures) {
                Write-Host "Installing $Kind features for GPU SM$architecture (credentials are not logged)."
                if ($Kind -eq 'Video') {
                    & $shell -NoProfile -ExecutionPolicy Bypass -File $vendor -gpu $architecture -features $effects -version 1.3.0.0
                } else {
                    $name = @{'75'='turing';'86'='ampere';'89'='ada';'120'='blackwell'}[$architecture]
                    & $shell -NoProfile -ExecutionPolicy Bypass -File $vendor -GPU_Architecture $name -Effects $effects -Version 3.0.0 -Output_Directory $features
                }
                if ($LASTEXITCODE -ne 0) { throw "$Kind feature installation failed for SM$architecture ($LASTEXITCODE)." }
            }
        }
    } finally { Pop-Location }
} finally { $env:NGC_CLI_API_KEY = $previousKey }
