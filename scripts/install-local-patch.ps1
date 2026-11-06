[CmdletBinding()]
param([switch]$Apply)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2
$project=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$build=Join-Path $project 'build'
$program=if($env:ProgramW6432){$env:ProgramW6432}else{$env:ProgramFiles}
$common=if($env:CommonProgramW6432){$env:CommonProgramW6432}else{$env:CommonProgramFiles}
$root=Join-Path $program '4x4Tools\BroadcastSuite'
$receiptPath=Join-Path $root 'install-receipt.json'
$manifestPath=Join-Path $root 'package-manifest.json'
function Hash([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}
function NoLinks([string]$Path){
    for($p=[IO.Path]::GetFullPath($Path);$p;$p=[IO.Path]::GetDirectoryName($p)) {
        if((Test-Path -LiteralPath $p) -and ((Get-Item -LiteralPath $p -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw "Linked path is not allowed: $p"}
    }
}
function ClosedAdobe {
    if(Get-Process -Name AfterFX,'Adobe Premiere Pro','Adobe Media Encoder' -ErrorAction SilentlyContinue){throw 'Save and close Adobe applications before replacing plugins.'}
}
NoLinks $root
$receipt=Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
$manifest=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if($receipt.product -ne '4x4Tools Broadcast Suite' -or $manifest.product -ne $receipt.product -or $receipt.root -ine $root){throw 'The installed suite receipt is not recognized.'}
$sources=@{}
if(@($manifest.files | Where-Object path -eq 'plugins/video/4x4EyeContact.aex').Count -eq 1){
    $sources['plugins/video/4x4EyeContact.aex']=Join-Path $build 'bin\4x4EyeContact.aex'
}
foreach($name in @('BackgroundRemove','BackgroundBlur','StudioLight','VideoDenoise','AutoFrame','DetailRestore')){$sources['plugins/video/4x4'+$name+'.aex']=Join-Path $build ('bin\4x4'+$name+'.aex')}
foreach($name in @('NoiseClean','RoomEchoClean','VoiceClean','StudioVoicePreview','StudioVoiceHQ','VoiceRestore')){$sources['plugins/audio/4x4'+$name+'.aex']=Join-Path $build ('bin\4x4'+$name+'.aex')}
foreach($relative in @('Contents/x86_64-win/4x4Tools Audio.vst3','Contents/Resources/moduleinfo.json')){$sources['plugins/vst3/4x4Tools Audio.vst3/'+$relative]=Join-Path $build ('VST3/Release/4x4Tools Audio.vst3/'+$relative)}
foreach($name in @('AudioEffectsCli','VideoEffectsCli')){$sources['Tools/'+$name+'.exe']=Join-Path $build ('bin\'+$name+'.exe')}
$version=(Get-Item -LiteralPath $sources['plugins/video/4x4DetailRestore.aex']).VersionInfo.ProductVersion
if($version -notmatch '^\d+\.\d+\.\d+$'){throw 'The patch has no valid version.'}
$plan=[Collections.Generic.List[object]]::new()
foreach($relative in @($sources.Keys | Sort-Object)) {
    $source=$sources[$relative];NoLinks $source
    if(!(Test-Path -LiteralPath $source -PathType Leaf)){throw "Missing build output: $source"}
    $entry=@($manifest.files | Where-Object path -eq $relative)
    if($entry.Count -ne 1){throw "Installed manifest does not own $relative"}
    $target=Join-Path $root $relative;NoLinks $target
    if((Hash $target) -ne $entry[0].sha256){throw "An installed file changed outside Setup: $target"}
    $next=Hash $source
    $plan.Add(@{source=$source;target=$target;before=$entry[0].sha256;after=$next;relative=$relative})
    if($relative -match '^plugins/') {
        foreach($file in $receipt.installedFiles) {
            $basename=[IO.Path]::GetFileName($relative)
            if([IO.Path]::GetFileName($file.path) -ine $basename){continue}
            $allowed=$file.path.StartsWith((Join-Path $program 'Adobe\Common\Plug-ins\7.0\MediaCore\4x4Tools\BroadcastSuite\'),[StringComparison]::OrdinalIgnoreCase) -or
                $file.path.StartsWith((Join-Path $common 'VST3\4x4Tools Audio.vst3\'),[StringComparison]::OrdinalIgnoreCase) -or
                $file.path -match ('^'+[regex]::Escape((Join-Path $program 'Adobe\'))+'Adobe After Effects \d{4}[^\\]*\\Support Files\\Plug-ins\\4x4Tools\\BroadcastSuiteAudio\\[^\\]+$')
            if(!$allowed){throw 'Unexpected host plugin path in receipt.'}
            NoLinks $file.path
            if((Hash $file.path) -ne $file.sha256){throw "An Adobe plugin changed outside Setup: $($file.path)"}
            $plan.Add(@{source=$source;target=$file.path;before=$file.sha256;after=$next;relative=$relative})
        }
    }
}
if(@($plan | Group-Object target | Where-Object Count -gt 1).Count){throw 'Duplicate patch destination.'}
ClosedAdobe
Write-Output ("Validated local version {0}: {1} owned files; NVIDIA runtimes and configuration remain in place." -f $version,$plan.Count)
if(!$Apply){Write-Output 'Validation only. Use -Apply to install with a rollback backup.';return}
$backup=Join-Path $build ('local-install-backups\'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8))
$null=New-Item -ItemType Directory -Path $backup
$records=[Collections.Generic.List[object]]::new()
foreach($file in $plan) {
    $saved=Join-Path $backup ('{0:D3}.original' -f $records.Count)
    Copy-Item -LiteralPath $file.target -Destination $saved
    if((Hash $saved) -ne $file.before){throw 'Backup verification failed.'}
    $file.backup=$saved;$records.Add($file)
}
foreach($path in @($receiptPath,$manifestPath)) {
    $saved=Join-Path $backup ([IO.Path]::GetFileName($path))
    Copy-Item -LiteralPath $path -Destination $saved
    $records.Add(@{target=$path;backup=$saved;before=(Hash $path)})
}
[IO.File]::WriteAllText((Join-Path $backup 'rollback.json'),($records | ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))
try {
    ClosedAdobe
    foreach($file in $plan) {
        if((Hash $file.source) -ne $file.after -or (Hash $file.target) -ne $file.before){throw 'A file changed after validation.'}
        Copy-Item -LiteralPath $file.source -Destination $file.target -Force
        if((Hash $file.target) -ne $file.after){throw "Installed file verification failed: $($file.target)"}
        foreach($entry in $manifest.files | Where-Object path -eq $file.relative){$entry.sha256=$file.after;$entry.bytes=(Get-Item -LiteralPath $file.source).Length}
        foreach($entry in $receipt.installedFiles | Where-Object path -eq $file.target){$entry.sha256=$file.after}
    }
    $baseVersion=$manifest.version;$manifest.version=$version;$receipt.version=$version
    $local=@{version=$version;baseVersion=$baseVersion;installedUtc=[DateTime]::UtcNow.ToString('o');backup=$backup}
    $manifest | Add-Member -NotePropertyName localPatch -NotePropertyValue $local -Force
    $receipt | Add-Member -NotePropertyName localPatch -NotePropertyValue $local -Force
    [IO.File]::WriteAllText($manifestPath,($manifest | ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($receiptPath,($receipt | ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
    Write-Output "Installed and hash-verified version $version. Backup: $backup"
} catch {
    $failure=$_
    foreach($record in $records){Copy-Item -LiteralPath $record.backup -Destination $record.target -Force}
    throw $failure
}
