[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$lock=Get-Content -LiteralPath (Join-Path $root 'dependencies.lock.json') -Raw | ConvertFrom-Json
$destination=Join-Path $root 'third_party\vst3sdk'
$previousLfs=$env:GIT_LFS_SKIP_SMUDGE
try {
    $env:GIT_LFS_SKIP_SMUDGE='1'
    if (!(Test-Path -LiteralPath (Join-Path $destination '.git'))) {
        $null=New-Item -ItemType Directory -Path $destination -Force
        & git -C $destination init --quiet
        if ($LASTEXITCODE -ne 0) { throw 'Git initialization failed.' }
        & git -C $destination remote add origin $lock.vst3.repository
        if ($LASTEXITCODE -ne 0) { throw 'Git remote configuration failed.' }
        & git -C $destination fetch --depth 1 origin $lock.vst3.commit
        if ($LASTEXITCODE -ne 0) { throw 'Pinned VST3 SDK fetch failed.' }
        & git -C $destination checkout --detach FETCH_HEAD
        if ($LASTEXITCODE -ne 0) { throw 'Pinned VST3 SDK checkout failed.' }
    }
    $actual=(& git -C $destination rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $actual -ne $lock.vst3.commit) { throw 'Existing VST3 checkout differs from dependencies.lock.json; preserve it and use a separate dependency directory.' }
    & git -C $destination submodule update --init --depth 1 base pluginterfaces public.sdk cmake
    if ($LASTEXITCODE -ne 0) { throw 'VST3 submodule fetch failed.' }
    foreach ($property in $lock.vst3.submodules.PSObject.Properties) {
        $actual=(& git -C (Join-Path $destination $property.Name) rev-parse HEAD).Trim()
        if ($LASTEXITCODE -ne 0 -or $actual -ne $property.Value) { throw "VST3 submodule differs from the lock: $($property.Name)" }
    }
    Write-Host 'Pinned VST3 SDK and required submodules verified.'
} finally { $env:GIT_LFS_SKIP_SMUDGE=$previousLfs }
