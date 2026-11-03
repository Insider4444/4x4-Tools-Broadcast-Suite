[CmdletBinding()]
param(
    [string]$AdobeSdk,
    [string]$ArSdk,
    [string]$VfxSdk,
    [string]$AfxSdk,
    [switch]$SkipTests,
    [switch]$WithVstValidator
)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
. (Join-Path $PSScriptRoot 'sdk-paths.ps1')
$paths=Get-4x4SdkPaths -Adobe $AdobeSdk -Ar $ArSdk -Vfx $VfxSdk -Afx $AfxSdk
$AdobeSdk=$paths.adobe;$ArSdk=$paths.ar;$VfxSdk=$paths.vfx;$AfxSdk=$paths.afx
$AdobeSdk = (Resolve-Path -LiteralPath $AdobeSdk).Path
$ArSdk = (Resolve-Path -LiteralPath $ArSdk).Path
$VfxSdk = (Resolve-Path -LiteralPath $VfxSdk).Path
$AfxSdk = (Resolve-Path -LiteralPath $AfxSdk).Path
$cmake = Join-Path $projectRoot '.tools\python\cmake\data\bin\cmake.exe'
if (!(Test-Path -LiteralPath $cmake)) {
    $cmake = (Get-Command cmake.exe -ErrorAction SilentlyContinue).Source
}
if (!$cmake) { throw 'CMake is missing. See README.md for the project-local bootstrap command.' }
if (!(Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $vsRoot = if (Test-Path -LiteralPath $vswhere) { & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath }
    $vcvars = if ($vsRoot) { Join-Path $vsRoot 'VC\Auxiliary\Build\vcvars64.bat' }
    if (!$vcvars -or !(Test-Path -LiteralPath $vcvars)) {
        $vcvars = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
    }
    if (!(Test-Path -LiteralPath $vcvars)) { throw 'MSVC x64 build tools are missing. Run from a Visual Studio x64 Developer PowerShell.' }
    $buildEnvironment = & $env:ComSpec /d /s /c "`"$vcvars`" >nul && set"
    if ($LASTEXITCODE -ne 0) { throw 'Could not initialize the MSVC build environment.' }
    foreach ($line in $buildEnvironment) {
        if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') }
    }
}
& $cmake -S $projectRoot -B (Join-Path $projectRoot 'build') -G 'NMake Makefiles' -DCMAKE_BUILD_TYPE=Release "-DAE_SDK_ROOT=$AdobeSdk" "-DAR_SDK_ROOT=$ArSdk" "-DVFX_SDK_ROOT=$VfxSdk" "-DAFX_SDK_ROOT=$AfxSdk"
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
& $cmake --build (Join-Path $projectRoot 'build') --config Release
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed.' }
if ($WithVstValidator) {
    & $cmake --build (Join-Path $projectRoot 'build') --config Release --target validator
    if ($LASTEXITCODE -ne 0) { throw 'VST3 validator build failed.' }
}
if (!$SkipTests) {
    $ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
    & $ctest --test-dir (Join-Path $projectRoot 'build') --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
}
