"""Exercise installer file transactions in a small, isolated Program Files tree.

Private script copies inject fixture directories and an empty process enumerator.
The distributed scripts retain their real Program Files guards. No real Adobe
directory, process, environment setting or registry key is modified.
"""
from pathlib import Path
import hashlib
import json
import subprocess
import tempfile
import zipfile

ROOT=Path(__file__).resolve().parents[1]


def ps_string(value):
    return "'"+str(value).replace("'","''")+"'"


def main():
    fixture=Path(tempfile.mkdtemp(prefix='installer-fixture-',dir=ROOT/'build')).resolve()
    if not fixture.is_relative_to((ROOT/'build').resolve()):
        raise RuntimeError('Fixture escaped the build tree')
    program=fixture/'ProgramFiles';common=fixture/'CommonFiles';package=fixture/'Package'
    for p in (program,common,package/'Data'):p.mkdir(parents=True,exist_ok=True)
    scripts={}
    for name in ('install','uninstall'):
        source=(ROOT/f'installer/{name}-suite.ps1').read_text(encoding='utf-8-sig')
        replacements={
            "$program64=if ($env:ProgramW6432) {$env:ProgramW6432} else {$env:ProgramFiles}":f'$program64={ps_string(program)}',
            "$common64=if ($env:CommonProgramW6432) {$env:CommonProgramW6432} else {$env:CommonProgramFiles}":f'$common64={ps_string(common)}',
        }
        for old,new in replacements.items():
            if source.count(old)!=1:raise RuntimeError('Installer fixture injection point changed')
            source=source.replace(old,new)
        source=source.replace('Get-Process -Name','Get-4x4FixtureProcess -Name')
        source=source.replace("$ErrorActionPreference='Stop'","function Get-4x4FixtureProcess { param([string[]]$Name,[string]$ErrorAction) }\n$ErrorActionPreference='Stop'",1)
        scripts[name]=fixture/f'{name}.ps1';scripts[name].write_text(source,encoding='utf-8-sig')
    contents={
        'runtime/VideoFX/bin/runtime.dll':(b'fixture runtime','video'),
        'runtime/AudioFX/bin/runtime.dll':(b'fixture audio runtime','audio'),
        'runtime/ARSDK/bin/models/fixture_100.engine.trtpkg':(b'fixture model','video'),
        'uninstall-suite.ps1':(scripts['uninstall'].read_bytes(),'common'),
        'Tools/SupportCheck.exe':(b'fixture diagnostics','common'),
        'README.md':(b'fixture guide','common'),
    }
    for target in ('EyeContact','BackgroundRemove','BackgroundBlur','StudioLight','VideoDenoise','AutoFrame','DetailRestore'):
        contents[f'plugins/video/4x4{target}.aex']=(f'fixture {target}'.encode(),'video')
    for target in ('NoiseClean','RoomEchoClean','VoiceClean','StudioVoicePreview','StudioVoiceHQ','VoiceRestore'):
        contents[f'plugins/audio/4x4{target}.aex']=(f'fixture {target}'.encode(),'audio')
    for relative in ('Contents/x86_64-win/4x4Tools Audio.vst3','Contents/Resources/moduleinfo.json','PlugIn.ico','desktop.ini'):
        contents[f'plugins/vst3/4x4Tools Audio.vst3/{relative}']=(b'fixture vst data','audio')
    files=[];blobs={}
    for relative,(content,component) in contents.items():
        digest=hashlib.sha256(content).hexdigest();blobs[digest]=content
        files.append(dict(path=relative,payload='plugins',sha256=digest,bytes=len(content),component=component))
    archive=package/'Data/plugins.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
        for digest,content in blobs.items():z.writestr(digest,content)
    manifest=dict(schema=1,product='4x4Tools Broadcast Suite',version='fixture',files=files,
        payloads=[dict(id='plugins',file='Data/plugins.zip',sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),bytes=archive.stat().st_size)])
    path=package/'manifest.json';path.write_text(json.dumps(manifest),encoding='utf-8')
    for kind in ('corrupt','escape'):
        changed=json.loads(json.dumps(manifest))
        if kind=='corrupt':changed['payloads'][0]['sha256']='0'*64
        else:changed['files'][0]['path']='../outside.txt'
        (package/f'{kind}.json').write_text(json.dumps(changed),encoding='utf-8')
    runner=fixture/'run.ps1'
    runner.write_text(r'''$ErrorActionPreference='Stop'
function Assert([bool]$Condition,[string]$Message) { if(!$Condition) {throw $Message} }
$base=Split-Path -Parent $PSCommandPath
$program=Join-Path $base 'ProgramFiles';$common=Join-Path $base 'CommonFiles';$package=Join-Path $base 'Package'
$root=Join-Path $program '4x4Tools\BroadcastSuite';$installer=Join-Path $base 'install.ps1';$uninstaller=Join-Path $base 'uninstall.ps1'
$manifest=Join-Path $package 'manifest.json'
$video=Join-Path $program 'Adobe\Common\Plug-ins\7.0\MediaCore\4x4Tools\BroadcastSuite'
$legacy=Join-Path $program 'Adobe\Common\Plug-ins\7.0\MediaCore\4x4Tools\EyeContact'
$other=Join-Path $program 'Adobe\Common\Plug-ins\7.0\MediaCore\4x4Tools\OtherTool\keep.txt'
$null=New-Item -ItemType Directory -Path $legacy,(Split-Path -Parent $other),(Join-Path $program 'Adobe\Adobe After Effects 2026\Support Files\Plug-ins') -Force
[IO.File]::WriteAllText((Join-Path $legacy '4x4EyeContact.aex'),'standalone eye sentinel')
[IO.File]::WriteAllText($other,'other tool sentinel')
foreach($case in @('corrupt','escape')) {
    $caught=$false
    try { & $installer -PackageRoot $package -Manifest (Join-Path $package "$case.json") -Architecture 120 -ValidateOnly }
    catch { $caught=$true }
    Assert $caught "Bad $case manifest was accepted"
    Assert (!(Test-Path -LiteralPath $root)) 'Failed verification changed the live product path'
}
& $installer -PackageRoot $package -Manifest $manifest -Architecture 120 -InstallRoot $root
Assert (Test-Path -LiteralPath (Join-Path $video '4x4BackgroundBlur.aex')) 'Video effect was not deployed'
Assert (!(Test-Path -LiteralPath (Join-Path $video '4x4EyeContact.aex'))) 'Standalone Eye Contact was duplicated'
Assert (Test-Path -LiteralPath (Join-Path $program 'Adobe\Adobe After Effects 2026\Support Files\Plug-ins\4x4Tools\BroadcastSuiteAudio\4x4VoiceClean.aex')) 'Native AE audio was not deployed'
Assert (Test-Path -LiteralPath (Join-Path $common 'VST3\4x4Tools Audio.vst3\Contents\x86_64-win\4x4Tools Audio.vst3')) 'VST3 was not deployed'
[IO.File]::WriteAllText((Join-Path $root 'user-notes.txt'),'preserve previous user addition')
& $installer -PackageRoot $package -Manifest $manifest -Architecture 120 -InstallRoot $root
$saved=@(Get-ChildItem -LiteralPath (Split-Path -Parent $root) -Directory -Filter '.BroadcastSuite.backup-*' | Where-Object {Test-Path -LiteralPath (Join-Path $_.FullName 'user-notes.txt')})
Assert ($saved.Count -eq 1) 'Upgrade discarded an unrelated file'
$caught=$false
try {& $installer -PackageRoot $package -Manifest $manifest -Architecture 120 -InstallRoot $root -Components video} catch {$caught=$true}
Assert $caught 'Component changes could orphan the previous audio installation'
$modified=Join-Path $video '4x4BackgroundBlur.aex';[IO.File]::AppendAllText($modified,' user modification')
& $uninstaller -InstallRoot $root
Assert (Test-Path -LiteralPath $modified) 'Uninstall discarded a modified plugin'
Assert (!(Test-Path -LiteralPath (Join-Path $video '4x4BackgroundRemove.aex'))) 'Uninstall retained an unchanged plugin'
Assert (([IO.File]::ReadAllText((Join-Path $legacy '4x4EyeContact.aex'))) -eq 'standalone eye sentinel') 'Standalone Eye Contact was changed'
Assert (([IO.File]::ReadAllText($other)) -eq 'other tool sentinel') 'Another 4x4Tools product was changed'
if((Test-Path -LiteralPath $root) -and !@(Get-ChildItem -LiteralPath $root -Force).Count) {Remove-Item -LiteralPath $root}
$block=Join-Path $common 'VST3\4x4Tools Audio.vst3'
Assert (!(Test-Path -LiteralPath $block)) 'VST3 bundle was not removed'
[IO.File]::WriteAllText($block,'force deployment failure')
$before=[IO.File]::ReadAllText($modified);$caught=$false
try {& $installer -PackageRoot $package -Manifest $manifest -Architecture 120 -InstallRoot $root} catch {$caught=$true}
Assert $caught 'Injected deployment failure was not detected'
Assert (!(Test-Path -LiteralPath $root)) 'A failed fresh installation blocked retries'
Assert (([IO.File]::ReadAllText($modified)) -eq $before) 'Rollback did not restore a prior modified plugin'
Write-Output 'PASS: integrity rejection, path rejection, deployment, upgrade, preserved user additions, component guard, uninstall and fresh-install rollback.'
''',encoding='utf-8-sig')
    result=subprocess.run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(runner)],cwd=ROOT,text=True,capture_output=True)
    (ROOT/'build/installer-fixture.log').write_text(result.stdout+result.stderr,encoding='utf-8')
    print(result.stdout[-4000:]);print(result.stderr[-1500:])
    print('Fixture:',fixture)
    if result.returncode:raise SystemExit(result.returncode)


if __name__=='__main__':main()
