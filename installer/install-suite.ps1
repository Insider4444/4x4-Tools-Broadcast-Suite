[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$PackageRoot,
    [Parameter(Mandatory=$true)][string]$Manifest,
    [Parameter(Mandatory=$true)][ValidateSet('75','86','89','120')][string]$Architecture,
    [string]$InstallRoot,
    [ValidateSet('all','video','audio')][string]$Components='all',
    [switch]$ValidateOnly,
    [switch]$StageOnly,
    [string]$StageRoot,
    [string]$ArSdkRoot,
    [string]$VfxSdkRoot,
    [string]$AfxSdkRoot,
    [string]$KeyFile,
    [string]$PayloadCache,
    [string]$LogFile
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2
$script:setupTranscript=$false
trap {
    if($script:setupTranscript) {
        Write-Output ("Setup stopped: "+$_.Exception.Message)
        Write-Output $_.InvocationInfo.PositionMessage
        Write-Output $_.ScriptStackTrace
        try { Stop-Transcript | Out-Null } catch { }
        $script:setupTranscript=$false
    }
    throw
}

function Full([string]$Path) { [IO.Path]::GetFullPath($Path).TrimEnd('\','/') }
function Child([string]$Parent,[string]$Relative) {
    if ([IO.Path]::IsPathRooted($Relative) -or $Relative -match '(^|[\\/])\.\.([\\/]|$)|:') { throw 'Unsafe package path.' }
    $prefix=(Full $Parent)+'\'; $result=Full (Join-Path $Parent $Relative)
    if (!$result.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)) { throw 'Path escapes its package directory.' }
    return $result
}
function NoReparse([string]$Path) {
    $current=Full $Path
    while ($current) {
        if ((Test-Path -LiteralPath $current) -and ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Installation paths cannot contain directory links: $current"
        }
        $parent=[IO.Path]::GetDirectoryName($current)
        if ($parent -eq $current) { break }; $current=$parent
    }
}
function Hash([string]$Path) {
    $algorithm=[Security.Cryptography.SHA256]::Create();$stream=[IO.File]::OpenRead($Path)
    try { return ([BitConverter]::ToString($algorithm.ComputeHash($stream))).Replace('-','').ToLowerInvariant() }
    finally { $stream.Dispose();$algorithm.Dispose() }
}
function DownloadPayload([string]$Url,[string]$Target,[long]$ExpectedBytes,[string]$Label='Required files') {
    # GitHub/CDN requests carry no credentials. A partial download can resume
    # after an interrupted connection; the embedded SHA-256 remains authoritative.
    [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
    $partial=$Target+'.download'
    NoReparse $Target;NoReparse $partial
    for($attempt=1;$attempt -le 3;$attempt++) {
        $response=$null;$input=$null;$output=$null
        try {
            $offset=if(Test-Path -LiteralPath $partial){(Get-Item -LiteralPath $partial).Length}else{0L}
            if($offset -eq $ExpectedBytes) {
                if(Test-Path -LiteralPath $Target){Remove-Item -LiteralPath $Target -Force}
                Move-Item -LiteralPath $partial -Destination $Target
                return
            }
            if($offset -gt $ExpectedBytes) {Remove-Item -LiteralPath $partial -Force;$offset=0L}
            $request=[Net.HttpWebRequest]::Create($Url)
            $request.UserAgent='4x4Tools-Setup/0.2.0';$request.Timeout=30000;$request.ReadWriteTimeout=60000
            if($offset -gt 0) {$request.AddRange([long]$offset)}
            $response=$request.GetResponse()
            $append=$offset -gt 0 -and [int]$response.StatusCode -eq 206
            if($append -and $response.Headers['Content-Range'] -notmatch "^bytes $offset-") {throw 'Invalid download resume response.'}
            $mode=if($append){[IO.FileMode]::Append}else{[IO.FileMode]::Create}
            NoReparse $Target;NoReparse $partial
            $output=[IO.File]::Open($partial,$mode,[IO.FileAccess]::Write,[IO.FileShare]::None)
            $input=$response.GetResponseStream()
            $buffer=[Array]::CreateInstance([byte],1048576)
            $done=if($append){$offset}else{0L};$lastReport=[DateTime]::UtcNow
            while(($read=$input.Read($buffer,0,$buffer.Length)) -gt 0) {
                $output.Write($buffer,0,$read);$done+=$read
                if(([DateTime]::UtcNow-$lastReport).TotalSeconds -ge 5) {
                    Write-Output ("{0}: {1:N0} of {2:N0} MB ({3:N0}%)" -f $Label,($done/1MB),($ExpectedBytes/1MB),(100*$done/$ExpectedBytes))
                    $lastReport=[DateTime]::UtcNow
                }
            }
            $output.Dispose();$output=$null
            $received=(Get-Item -LiteralPath $partial).Length
            if($received -ne $ExpectedBytes) {
                if($received -gt $ExpectedBytes) {Remove-Item -LiteralPath $partial -Force}
                throw 'Download length differs from the embedded manifest.'
            }
            if(Test-Path -LiteralPath $Target) {Remove-Item -LiteralPath $Target -Force}
            Move-Item -LiteralPath $partial -Destination $Target
            return
        } catch {
            if($attempt -eq 3) {throw "The download could not finish. Check your connection and run Setup again to resume automatically. $($_.Exception.Message)"}
            Write-Output "Connection interrupted; resuming download (attempt $($attempt+1)/3)..."
        } finally {
            if($input){$input.Dispose()};if($output){$output.Dispose()};if($response){$response.Dispose()}
        }
    }
}
function PayloadLabel([string]$Id) {
    switch -Regex ($Id) {
        '^models-.*-audio' {return 'Voice enhancement models'}
        '^models-.*-video' {return 'Picture enhancement models'}
        '^runtime-audio' {return 'NVIDIA voice processing libraries'}
        '^runtime-video' {return 'NVIDIA picture processing libraries'}
        '^plugins-audio' {return '4x4Tools voice effects'}
        '^plugins-video' {return '4x4Tools picture effects'}
        default {return 'Shared application files'}
    }
}
function WriteIni([string]$Directory,[string]$Runtime) {
    $video=if($script:community){$VfxSdkRoot}else{"$Runtime\runtime\VideoFX"}
    $audio=if($script:community){$AfxSdkRoot}else{"$Runtime\runtime\AudioFX"}
    $ar=if($script:community){$ArSdkRoot}else{"$Runtime\runtime\ARSDK"}
    [IO.File]::WriteAllText((Join-Path $Directory '4x4Tools.ini'),"[NVIDIA]`r`nVFXSDK_ROOT=$video`r`nAFXSDK_ROOT=$audio`r`n",[Text.Encoding]::Unicode)
    if (Test-Path -LiteralPath (Join-Path $Directory '4x4EyeContact.aex')) {
        [IO.File]::WriteAllText((Join-Path $Directory '4x4EyeContact.ini'),"[NVIDIA]`r`nARSDK_ROOT=$ar`r`n",[Text.Encoding]::Unicode)
    }
}

if($LogFile) {
    $LogFile=Full $LogFile;NoReparse $LogFile
    Start-Transcript -LiteralPath $LogFile -Append | Out-Null
    $script:setupTranscript=$true
}
Write-Output 'Starting 4x4Tools Setup...'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$PackageRoot=Full $PackageRoot
$data=Get-Content -LiteralPath $Manifest -Raw | ConvertFrom-Json
if ($data.schema -ne 1 -or $data.product -ne '4x4Tools Broadcast Suite') { throw 'Unrecognized 4x4Tools package manifest.' }
$script:community=$null -ne $data.PSObject.Properties['distribution'] -and $data.distribution -eq 'community-own-sdks'
$encrypted=$null -ne $data.PSObject.Properties['distribution'] -and $data.distribution -eq 'end-user-encrypted'
if($encrypted) {
    if(!$KeyFile -or !(Test-Path -LiteralPath $KeyFile)) {throw 'The installer release key is missing. Use the official Setup executable.'}
    if($data.releaseBase -notmatch '^https://github\.com/Insider4444/4x4-Tools-Broadcast-Suite/releases/download/v[0-9A-Za-z.\-]+$') {throw 'Unexpected release download location.'}
    # NSIS extracts a native plug-in named System.dll into its working folder.
    # Windows PowerShell's compiler otherwise resolves that file as a .NET
    # reference. Use the loaded framework assembly's absolute path instead.
    Add-Type -Path (Join-Path $PSScriptRoot 'payload-crypto.cs') -ReferencedAssemblies ([Uri].Assembly.Location)
}
if($script:community) {
    function RequireFile([string]$Root,[string]$Relative) {
        if(!$Root -or !(Test-Path -LiteralPath (Join-Path $Root $Relative) -PathType Leaf)) {throw "Your NVIDIA download is incomplete: $Relative. See the community setup guide; SDKs and models must come from your own NVIDIA account."}
    }
    $sm=@{'75'='75';'86'='86';'89'='89';'120'='100'}[$Architecture]
    if($Components -ne 'audio') {
        foreach($file in @('bin\nvARPose.dll','bin\NVCVImage.dll','bin\cudart64_12.dll',
            'features\nvargazeredirection\bin\nvARGazeRedirection.dll','features\nvarlandmarkdetection\bin\nvARLandmarkDetection.dll','features\nvarfaceboxdetection\bin\nvARFaceBoxDetection.dll')) {RequireFile $ArSdkRoot $file}
        foreach($prefix in @('gazeredir_encoder_fp16','gazeredir_decoder_fp16','face_detection','face_and_landmark_detector_119_fp32','faceland_126_maxbatch1_mode0','faceland_126_maxbatch1_mode1')) {
            RequireFile $ArSdkRoot "bin\models\${prefix}_${sm}.engine.trtpkg"
        }
        RequireFile $VfxSdkRoot 'bin\NVVideoEffects.dll';RequireFile $VfxSdkRoot 'bin\NVCVImage.dll'
        foreach($feature in @('GreenScreen','BackgroundBlur','Denoising','Relighting','VideoSuperRes','Upscale')) {
            RequireFile $VfxSdkRoot ("features\nvvfx$($feature.ToLowerInvariant())\bin\nvVFX$feature.dll")
        }
        RequireFile $VfxSdkRoot 'features\nvvfxvideosuperres\bin\nvngx_vsr.dll'
        foreach($mode in 0..3) {RequireFile $VfxSdkRoot "bin\models\AIGS_288x512_${sm}_m${mode}.engine.trtpkg"}
        foreach($name in @("Denoise-agg-$sm","Denoise-con-$sm","relight_0thframe_perf_$sm","relight_nthframe_perf_$sm")) {RequireFile $VfxSdkRoot "bin\models\$name.engine.trtpkg"}
        $ArSdkRoot=(Resolve-Path -LiteralPath $ArSdkRoot).Path;$VfxSdkRoot=(Resolve-Path -LiteralPath $VfxSdkRoot).Path
    }
    if($Components -ne 'video') {
        RequireFile $AfxSdkRoot 'bin\NVAudioEffects.dll'
        $audioArch=@{'75'='turing';'86'='ampere';'89'='ada';'120'='blackwell'}[$Architecture]
        $models=@{nvafxdenoiser='denoiser_v2_48k.trtpkg';nvafxdereverb='dereverb_48k.trtpkg';nvafxdereverbdenoiser='dereverb_denoiser_48k.trtpkg';
            nvafxstudiovoicehighquality='studio_voice_high_quality_48k.trtpkg';nvafxstudiovoicelowlatency='studio_voice_low_latency_48k.trtpkg';nvafxsuperres='superres_16kto48k.trtpkg'}
        foreach($feature in $models.Keys) {RequireFile $AfxSdkRoot "features\$feature\bin\$feature.dll";RequireFile $AfxSdkRoot "features\$feature\models\$audioArch\$($models[$feature])"}
        RequireFile $AfxSdkRoot "features\nvafxstudiovoicelowlatency\models\$audioArch\studio_voice_mic_profiles_48k.trtpkg"
        foreach($directory in @('bin\external\cuda\bin','bin\external\nvtrt\bin','bin\external\openssl\bin')) {
            if(!(Test-Path -LiteralPath (Join-Path $AfxSdkRoot $directory) -PathType Container)) {throw "Missing NVIDIA runtime folder: $directory"}
        }
        $AfxSdkRoot=(Resolve-Path -LiteralPath $AfxSdkRoot).Path
    }
    Write-Output 'Using your local NVIDIA SDKs and models in place. No NVIDIA files are copied into this package.'
}
if (!$StageOnly -and !$ValidateOnly -and $InstallRoot) {
    $existing=Join-Path $InstallRoot 'install-receipt.json'
    if (Test-Path -LiteralPath $existing) {
        $previous=Get-Content -LiteralPath $existing -Raw | ConvertFrom-Json
        if ($previous.product -eq $data.product -and $previous.components -ne $Components) {
            throw 'To change installed components, remove the previous 4x4Tools Broadcast Suite through Installed Apps, then run Setup again.'
        }
    }
}
$selected=@($data.files | Where-Object {
    ($_.payload -match "^(runtime|plugins|models-$Architecture)(-|$)") -and
    ($_.component -eq 'common' -or $Components -eq 'all' -or $_.component -eq $Components)
})
if (!$selected.Count) { throw 'No files selected.' }
$seen=@{}
foreach ($file in $selected) {
    if ($file.sha256 -notmatch '^[a-f0-9]{64}$' -or [long]$file.bytes -lt 0) { throw 'Invalid file digest or size.' }
    $null=Child $PackageRoot $file.path
    if ($seen.ContainsKey($file.path)) { throw 'Duplicate package destination.' }; $seen[$file.path]=$true
}
$payloads=@($data.payloads | Where-Object { @($selected.payload) -contains $_.id })
$archives=@{}
Write-Output 'Preparing and verifying the selected payloads...'
foreach ($payload in $payloads) {
    $label=PayloadLabel $payload.id
    Write-Output "Preparing $label..."
    $archive=Child $PackageRoot $payload.file
    NoReparse $archive
    if($encrypted -and !(Test-Path -LiteralPath $archive)) {
        if(!$PayloadCache) {$PayloadCache=Join-Path $env:ProgramData ("4x4Tools\Downloads\"+$data.version)}
        $PayloadCache=Full $PayloadCache;NoReparse $PayloadCache
        $null=New-Item -ItemType Directory -Path $PayloadCache -Force
        if($payload.asset -notmatch '^[a-zA-Z0-9.\-]+\.4x4data$') {throw 'Invalid release asset name.'}
        $archive=Child $PayloadCache $payload.asset
        if((Test-Path -LiteralPath $archive) -and ((Get-Item -LiteralPath $archive).Length -ne [long]$payload.bytes -or (Hash $archive) -ne $payload.sha256)) {
            Remove-Item -LiteralPath $archive -Force
        }
        if(!(Test-Path -LiteralPath $archive)) {
            Write-Output "Downloading $label ($([Math]::Round($payload.bytes/1MB)) MB)..."
            DownloadPayload ($data.releaseBase+'/'+$payload.asset) $archive ([long]$payload.bytes) $label
        }
    }
    if (!(Test-Path -LiteralPath $archive) -or (Get-Item -LiteralPath $archive).Length -ne [long]$payload.bytes -or (Hash $archive) -ne $payload.sha256) {
        throw "Required files could not be verified ($label). Run Setup again to retry the download automatically."
    }
    $archives[$payload.id]=$archive
}
if ($ValidateOnly) {
    Write-Output "Verified $($selected.Count) files for NVIDIA SM$Architecture."
    if($script:setupTranscript) { Stop-Transcript | Out-Null }
    exit 0
}

$program64=if ($env:ProgramW6432) {$env:ProgramW6432} else {$env:ProgramFiles}
$common64=if ($env:CommonProgramW6432) {$env:CommonProgramW6432} else {$env:CommonProgramFiles}
$expectedRoot=Join-Path $program64 '4x4Tools\BroadcastSuite'
if ($StageOnly) {
    if (!$StageRoot) { throw '-StageOnly requires a new, explicit StageRoot.' }
    $stage=Full $StageRoot; NoReparse $stage
    if (Test-Path -LiteralPath $stage) { throw 'StageRoot must be a new directory.' }
    $effectiveRoot=$stage
} else {
    if ((Full $InstallRoot) -ine (Full $expectedRoot)) { throw 'Unexpected installation directory.' }
    NoReparse $InstallRoot
    if (Get-Process -Name AfterFX,'Adobe Premiere Pro','Adobe Media Encoder' -ErrorAction SilentlyContinue) { throw 'Save and close Adobe applications before installing.' }
    $parent=Split-Path -Parent $expectedRoot
    $stage=Child $parent ('.BroadcastSuite.stage-'+[Guid]::NewGuid().ToString('N'))
    $effectiveRoot=Full $InstallRoot
}
$null=New-Item -ItemType Directory -Path $stage -ErrorAction Stop
$blobs=Child $stage '.payload-blobs'; $null=New-Item -ItemType Directory -Path $blobs
Write-Output 'Unpacking verified NVIDIA runtimes and models...'
$blobFiles=@{}
foreach ($payload in $payloads) {
    Write-Output "Installing $(PayloadLabel $payload.id)..."
    $needed=@{}
    foreach ($file in @($selected | Where-Object {$_.payload -eq $payload.id})) { $needed[$file.sha256]=[long]$file.bytes }
    $archive=$archives[$payload.id];$plain=$null
    if($encrypted) {
        $plain=Child $blobs ($payload.id+'.zip')
        [FourByFour.PayloadCrypto]::Decrypt($archive,$plain,[IO.File]::ReadAllBytes($KeyFile))
        $archive=$plain
    }
    $zip=[IO.Compression.ZipFile]::OpenRead($archive)
    try {
        $entries=@{}
        foreach ($entry in $zip.Entries) {
            if ($entry.FullName -notmatch '^[a-f0-9]{64}$' -or $entries.ContainsKey($entry.FullName)) { throw 'Invalid content-addressed archive entry.' }
            $entries[$entry.FullName]=$true
            if (!$needed.ContainsKey($entry.FullName) -or $blobFiles.ContainsKey($entry.FullName)) { continue }
            if ($entry.Length -ne $needed[$entry.FullName]) { throw 'Archive entry size differs from the embedded manifest.' }
            $target=Child $blobs $entry.FullName
            $input=$entry.Open();$output=[IO.File]::Create($target)
            try { $input.CopyTo($output) } finally { $output.Dispose();$input.Dispose() }
            if ((Hash $target) -ne $entry.FullName) { throw 'Extracted file failed its SHA-256 check.' }
            $blobFiles[$entry.FullName]=$target
        }
    } finally { $zip.Dispose();if($plain -and (Test-Path -LiteralPath $plain)){Remove-Item -LiteralPath $plain -Force} }
}
foreach ($file in $selected) {
    if (!$blobFiles.ContainsKey($file.sha256)) { throw "Missing archive entry for $($file.path)." }
    $target=Child $stage $file.path
    $null=New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force
    try { $null=New-Item -ItemType HardLink -Path $target -Target $blobFiles[$file.sha256] -ErrorAction Stop }
    catch { Copy-Item -LiteralPath $blobFiles[$file.sha256] -Destination $target -ErrorAction Stop }
}
foreach ($path in $blobFiles.Values) { Remove-Item -LiteralPath (Child $blobs ([IO.Path]::GetFileName($path))) -Force }
Remove-Item -LiteralPath $blobs
foreach ($directory in @('plugins\video','plugins\audio','plugins\vst3\4x4Tools Audio.vst3\Contents\x86_64-win')) {
    $path=Child $stage $directory
    if (Test-Path -LiteralPath $path) { WriteIni $path $effectiveRoot }
}
[IO.File]::WriteAllText((Child $stage 'package-manifest.json'),(Get-Content -LiteralPath $Manifest -Raw),[Text.UTF8Encoding]::new($false))
if ($StageOnly) {
    Write-Output "Stage verified and configured: $stage"
    if($script:setupTranscript) { Stop-Transcript | Out-Null }
    exit 0
}

# All hashing and extraction precede changes to installed plugin locations.
$backup=$null; $promoted=$false; $deployed=[Collections.Generic.List[object]]::new()
try {
    if (Test-Path -LiteralPath $InstallRoot) {
        $receipt=Child $InstallRoot 'install-receipt.json'
        if (!(Test-Path -LiteralPath $receipt) -or (Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json).product -ne $data.product) {
            throw 'The installation directory is not owned by this suite.'
        }
        $backup=Child (Split-Path -Parent $InstallRoot) ('.BroadcastSuite.backup-'+[Guid]::NewGuid().ToString('N'))
        NoReparse $InstallRoot; NoReparse $backup
        Move-Item -LiteralPath (Full $InstallRoot) -Destination $backup
    }
    # Both resolved paths are children of the fixed 4x4Tools Program Files root.
    $null=Child (Split-Path -Parent $expectedRoot) ([IO.Path]::GetFileName($stage))
    Move-Item -LiteralPath (Full $stage) -Destination (Full $InstallRoot)
    $promoted=$true
    $copies=[Collections.Generic.List[object]]::new()
    if ($Components -ne 'audio') {
        $destination=Join-Path $program64 'Adobe\Common\Plug-ins\7.0\MediaCore\4x4Tools\BroadcastSuite'
        $legacyEye=Join-Path $program64 'Adobe\Common\Plug-ins\7.0\MediaCore\4x4Tools\EyeContact\4x4EyeContact.aex'
        foreach ($file in Get-ChildItem -LiteralPath (Child $InstallRoot 'plugins\video') -File) {
            if ((Test-Path -LiteralPath $legacyEye) -and $file.Name -in @('4x4EyeContact.aex','4x4EyeContact.ini')) { continue }
            $copies.Add(@{source=$file.FullName;target=(Join-Path $destination $file.Name)})
        }
    }
    if ($Components -ne 'video') {
        $adobe=Join-Path $program64 'Adobe'
        if (Test-Path -LiteralPath $adobe) {
            foreach ($hostFolder in Get-ChildItem -LiteralPath $adobe -Directory) {
                if ($hostFolder.Name -notmatch '^Adobe After Effects \d{4}(?:\s|$)') { continue }
                $hostPlugins=Join-Path $hostFolder.FullName 'Support Files\Plug-ins'
                if (!(Test-Path -LiteralPath $hostPlugins)) { continue }
                $destination=Join-Path $hostPlugins '4x4Tools\BroadcastSuiteAudio'
                foreach ($file in Get-ChildItem -LiteralPath (Child $InstallRoot 'plugins\audio') -File) {
                    $copies.Add(@{source=$file.FullName;target=(Join-Path $destination $file.Name)})
                }
            }
        }
        $vstSource=Child $InstallRoot 'plugins\vst3\4x4Tools Audio.vst3'
        foreach ($file in Get-ChildItem -LiteralPath $vstSource -File -Recurse) {
            $relative=$file.FullName.Substring($vstSource.Length+1)
            $copies.Add(@{source=$file.FullName;target=(Child (Join-Path $common64 'VST3\4x4Tools Audio.vst3') $relative)})
        }
    }
    foreach ($copy in $copies) {
        NoReparse $copy.target
        $old=$null
        if (Test-Path -LiteralPath $copy.target) {
            $old=Child $InstallRoot ('rollback\'+$deployed.Count.ToString()+'.bin')
            $null=New-Item -ItemType Directory -Path (Split-Path -Parent $old) -Force
            Copy-Item -LiteralPath $copy.target -Destination $old
        }
        $record=@{path=$copy.target;previous=$old};$deployed.Add($record)
        $null=New-Item -ItemType Directory -Path (Split-Path -Parent $copy.target) -Force
        Copy-Item -LiteralPath $copy.source -Destination $copy.target -Force
        $record.sha256=Hash $copy.target
    }
    $installed=@($deployed | ForEach-Object { @{path=$_.path;sha256=$_.sha256} })
    $receipt=@{product=$data.product;version=$data.version;root=(Full $InstallRoot);architecture=$Architecture;components=$Components;installedFiles=$installed;
        distribution=$(if($encrypted){'end-user-encrypted'}elseif($script:community){'community-own-sdks'}else{'offline-local'});sdkRoots=@{ar=$ArSdkRoot;vfx=$VfxSdkRoot;afx=$AfxSdkRoot}}
    [IO.File]::WriteAllText((Child $InstallRoot 'install-receipt.json'),($receipt | ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
} catch {
    foreach ($record in $deployed) {
        if ($record.previous -and (Test-Path -LiteralPath $record.previous)) { Copy-Item -LiteralPath $record.previous -Destination $record.path -Force }
        elseif (Test-Path -LiteralPath $record.path) { Remove-Item -LiteralPath $record.path -Force }
    }
    if ($backup) {
        $failed=Child (Split-Path -Parent $InstallRoot) ('.BroadcastSuite.failed-'+[Guid]::NewGuid().ToString('N'))
        NoReparse $InstallRoot;NoReparse $failed
        if (Test-Path -LiteralPath $InstallRoot) { Move-Item -LiteralPath (Full $InstallRoot) -Destination $failed }
        Move-Item -LiteralPath (Full $backup) -Destination (Full $InstallRoot)
    } elseif ($promoted -and (Test-Path -LiteralPath $InstallRoot)) {
        # This is the new verified stage, not a pre-existing unowned directory.
        # Retain it away from the live product path so retry works.
        $failed=Child (Split-Path -Parent $InstallRoot) ('.BroadcastSuite.failed-'+[Guid]::NewGuid().ToString('N'))
        NoReparse $InstallRoot;NoReparse $failed
        Move-Item -LiteralPath (Full $InstallRoot) -Destination $failed
        Write-Warning "Verified staging files were retained at $failed"
    }
    throw
}
# Cleanup cannot turn a committed installation into an unregistered failure.
try {
# Retire only verified files from the previous suite, preserving user additions.
foreach ($record in $deployed) {
    if ($record.previous -and (Test-Path -LiteralPath $record.previous)) {
        $safe=Child (Child $InstallRoot 'rollback') ([IO.Path]::GetFileName($record.previous))
        Remove-Item -LiteralPath $safe -Force
    }
}
$rollback=Child $InstallRoot 'rollback'
if ((Test-Path -LiteralPath $rollback) -and !@(Get-ChildItem -LiteralPath $rollback -Force).Count) { Remove-Item -LiteralPath $rollback }
if ($backup) {
    $parent=Split-Path -Parent $expectedRoot
    $safe=Child $parent ([IO.Path]::GetFileName($backup))
    if ([IO.Path]::GetFileName($safe) -notlike '.BroadcastSuite.backup-*') { throw 'Unexpected backup cleanup path.' }
    NoReparse $safe
    if (@(Get-ChildItem -LiteralPath $safe -Recurse -Force | Where-Object {$_.Attributes -band [IO.FileAttributes]::ReparsePoint}).Count) {
        Write-Warning "Previous installation contains a directory link; backup preserved at $safe"
    } else {
        $previousManifest=Get-Content -LiteralPath (Child $safe 'package-manifest.json') -Raw | ConvertFrom-Json
        if ($previousManifest.product -ne $data.product) { throw 'Unrecognized backup manifest.' }
        foreach ($file in $previousManifest.files) {
            $path=Child $safe $file.path
            if ((Test-Path -LiteralPath $path) -and (Hash $path) -eq $file.sha256) { Remove-Item -LiteralPath $path -Force }
        }
        foreach ($relative in @('plugins\video\4x4Tools.ini','plugins\video\4x4EyeContact.ini','plugins\audio\4x4Tools.ini',
                'plugins\vst3\4x4Tools Audio.vst3\Contents\x86_64-win\4x4Tools.ini','package-manifest.json','install-receipt.json','Uninstall.exe')) {
            $path=Child $safe $relative
            if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Force }
        }
        foreach ($directory in @(Get-ChildItem -LiteralPath $safe -Directory -Recurse -Force | Sort-Object {$_.FullName.Length} -Descending)) {
            if (!@(Get-ChildItem -LiteralPath $directory.FullName -Force).Count) { Remove-Item -LiteralPath $directory.FullName }
        }
        if (!@(Get-ChildItem -LiteralPath $safe -Force).Count) { Remove-Item -LiteralPath $safe }
        else { Write-Warning "Modified or additional files from the previous installation are preserved at $safe" }
    }
}
} catch { Write-Warning "The suite is installed. Previous files were preserved because cleanup could not finish: $($_.Exception.Message)" }
Write-Output '4x4Tools Broadcast Suite installed successfully.'
if($script:setupTranscript) { Stop-Transcript | Out-Null }
