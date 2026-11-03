# Local settings are ignored by Git. No credentials are stored in this file.
function Get-4x4SdkPaths {
    param([string]$Adobe,[string]$Ar,[string]$Vfx,[string]$Afx)
    $project=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
    $settings=@{}
    $config=Join-Path $project 'sdk-paths.local.json'
    if(Test-Path -LiteralPath $config) {
        $value=Get-Content -LiteralPath $config -Raw | ConvertFrom-Json
        foreach($p in $value.PSObject.Properties) {$settings[$p.Name]=$p.Value}
    }
    $values=@{
        adobe=@($Adobe,$env:AE_SDK_ROOT,$settings['adobe'],(Join-Path $project 'third_party\adobe-ae-sdk'))
        ar=@($Ar,$env:AR_SDK_ROOT,$settings['ar'],(Join-Path $project 'ARSDK_windows_1.2.0.0\ARSDK'))
        vfx=@($Vfx,$env:VFX_SDK_ROOT,$settings['vfx'],(Join-Path $project 'VFXSDK_windows_1.3.0.0\VideoFX'))
        afx=@($Afx,$env:AFX_SDK_ROOT,$settings['afx'],(Join-Path $project '2026-08-24_NVIDIA_AFX_SDK_Win_Core_v3.0.0.51_x64'))
    }
    $result=@{}
    foreach($name in $values.Keys) {$result[$name]=@($values[$name] | Where-Object {$_})[0]}
    return $result
}
