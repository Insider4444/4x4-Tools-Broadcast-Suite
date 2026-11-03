[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$InstallRoot)
$ErrorActionPreference='Stop'
$program64=if ($env:ProgramW6432) {$env:ProgramW6432} else {$env:ProgramFiles}
$common64=if ($env:CommonProgramW6432) {$env:CommonProgramW6432} else {$env:CommonProgramFiles}
$root=[IO.Path]::GetFullPath($InstallRoot).TrimEnd('\')
if ($root -ine (Join-Path $program64 '4x4Tools\BroadcastSuite')) { throw 'Unexpected suite removal directory.' }
if ((Get-Item -LiteralPath $root -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Removal directory is a link.' }
if (Get-Process -Name AfterFX,'Adobe Premiere Pro','Adobe Media Encoder' -ErrorAction SilentlyContinue) { throw 'Save and close Adobe applications before uninstalling.' }
$receipt=Get-Content -LiteralPath (Join-Path $root 'install-receipt.json') -Raw | ConvertFrom-Json
$manifest=Get-Content -LiteralPath (Join-Path $root 'package-manifest.json') -Raw | ConvertFrom-Json
if ($receipt.product -ne '4x4Tools Broadcast Suite' -or $receipt.root -ine $root -or $manifest.product -ne $receipt.product) { throw 'Unrecognized suite receipt.' }
$directories=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
function Hash([string]$Path) {
    $algorithm=[Security.Cryptography.SHA256]::Create();$stream=[IO.File]::OpenRead($Path)
    try { return ([BitConverter]::ToString($algorithm.ComputeHash($stream))).Replace('-','').ToLowerInvariant() }
    finally { $stream.Dispose();$algorithm.Dispose() }
}
function NoLinks([string]$Path) {
    $current=$Path
    while ($current) {
        if ((Test-Path -LiteralPath $current) -and ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Removal path contains a link.' }
        $current=[IO.Path]::GetDirectoryName($current)
    }
}
function Owned([string]$Path) {
    $p=[IO.Path]::GetFullPath($Path)
    $video=Join-Path $program64 'Adobe\Common\Plug-ins\7.0\MediaCore\4x4Tools\BroadcastSuite\'
    $vst=Join-Path $common64 'VST3\4x4Tools Audio.vst3\'
    $audioPattern='^'+[regex]::Escape((Join-Path $program64 'Adobe\'))+'Adobe After Effects \d{4}[^\\]*\\Support Files\\Plug-ins\\4x4Tools\\BroadcastSuiteAudio\\[^\\]+$'
    if (!$p.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -and !$p.StartsWith($video,[StringComparison]::OrdinalIgnoreCase) -and
        !$p.StartsWith($vst,[StringComparison]::OrdinalIgnoreCase) -and $p -notmatch $audioPattern) { throw 'Receipt contains an unexpected plugin path.' }
    NoLinks $p;return $p
}
foreach ($file in $receipt.installedFiles) {
    $p=Owned $file.path
    if (Test-Path -LiteralPath $p) {
        if ((Hash $p) -ieq $file.sha256) { Remove-Item -LiteralPath $p -Force }
        else { Write-Warning "Preserving a modified file: $p" }
    }
    $null=$directories.Add((Split-Path -Parent $p))
}
foreach ($file in $manifest.files) {
    if ([IO.Path]::IsPathRooted($file.path) -or $file.path -match '(^|[\\/])\.\.([\\/]|$)|:') { throw 'Unsafe runtime manifest path.' }
    $p=Owned (Join-Path $root $file.path)
    if (Test-Path -LiteralPath $p) {
        if ((Hash $p) -ieq $file.sha256) { Remove-Item -LiteralPath $p -Force }
        else { Write-Warning "Preserving a modified file: $p" }
    }
    $null=$directories.Add((Split-Path -Parent $p))
}
# Generated configuration files and this receipt belong to the suite as well.
foreach ($relative in @('plugins\video\4x4Tools.ini','plugins\video\4x4EyeContact.ini','plugins\audio\4x4Tools.ini','plugins\vst3\4x4Tools Audio.vst3\Contents\x86_64-win\4x4Tools.ini')) {
    $target=Owned (Join-Path $root $relative)
    if (Test-Path -LiteralPath $target) {Remove-Item -LiteralPath $target -Force;$null=$directories.Add((Split-Path -Parent $target))}
}
foreach ($name in @('install-receipt.json','package-manifest.json')) { Remove-Item -LiteralPath (Owned (Join-Path $root $name)) -Force }
foreach ($directory in @($directories | Sort-Object Length -Descending)) {
    $current=$directory
    while ($current -and $current -ine $root -and
        ($current.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or
         ([IO.Path]::GetFileName($current) -notin @('4x4Tools','VST3','MediaCore','Plug-ins','Adobe')))) {
        NoLinks $current
        if (!(Test-Path -LiteralPath $current) -or @(Get-ChildItem -LiteralPath $current -Force).Count) { break }
        Remove-Item -LiteralPath $current
        $current=Split-Path -Parent $current
    }
}
Write-Output '4x4Tools suite files removed. Modified or unrelated files were preserved.'
