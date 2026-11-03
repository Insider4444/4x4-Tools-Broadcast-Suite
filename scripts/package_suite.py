"""Create deduplicated ZIP64 payloads from explicit runtime allowlists.

Each payload contains content-addressed blobs. The manifest, embedded in Setup,
pins every archive and installed file. No credentials or development INIs enter
the distribution. Identical runtime DLLs are stored only once per payload.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[1]
VERSION = '0.2.0-preview.1'
OUT = ROOT / 'dist' / f'4x4Tools-Broadcast-Suite-{VERSION}'
AR = ROOT / 'ARSDK_windows_1.2.0.0' / 'ARSDK'
VFX = ROOT / 'VFXSDK_windows_1.3.0.0' / 'VideoFX'
AFX = ROOT / '2026-08-24_NVIDIA_AFX_SDK_Win_Core_v3.0.0.51_x64'
ARCHITECTURES = {'75': ('75','turing'), '86': ('86','ampere'), '89': ('89','ada'), '120': ('100','blackwell')}
VIDEO = ['EyeContact','BackgroundRemove','BackgroundBlur','StudioLight','VideoDenoise','AutoFrame','DetailRestore']
AUDIO = ['NoiseClean','RoomEchoClean','VoiceClean','StudioVoicePreview','StudioVoiceHQ','VoiceRestore']
VFX_FEATURES = ['nvvfxgreenscreen','nvvfxbackgroundblur','nvvfxdenoising','nvvfxrelighting','nvvfxupscale','nvvfxvideosuperres']
AFX_FEATURES = ['nvafxdenoiser','nvafxdereverb','nvafxdereverbdenoiser','nvafxstudiovoicehighquality','nvafxstudiovoicelowlatency','nvafxsuperres']
FILES = []
INPUTS = {}


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(8*1024*1024), b''):
            h.update(block)
    return h.hexdigest()


def add(path, destination, payload='runtime', component='common'):
    if not path.is_file():
        raise RuntimeError(f'Missing package dependency: {path}')
    relative = Path(destination).as_posix()
    if relative.startswith('/') or '..' in Path(relative).parts or ':' in relative or any(p.startswith('.env') for p in Path(relative).parts):
        raise RuntimeError('Rejected private or unsafe package path')
    sha = digest(path)
    FILES.append(dict(path=relative, payload=payload, sha256=sha, bytes=path.stat().st_size, component=component))
    INPUTS.setdefault(payload, {})[sha] = path


def licenses(sdk, destination, component):
    for base in ('license','licenses'):
        for path in (sdk/base).rglob('*'):
            if path.is_file() and path.suffix.lower() in ('.pdf','.txt','.md','.html'):
                add(path, f'{destination}/{path.relative_to(sdk).as_posix()}', component=component)


def collect(runtime_only=False):
    for sdk, name, component in ((AR,'ARSDK','video'), (VFX,'VideoFX','video')):
        dest = f'runtime/{name}'
        for path in (sdk/'bin').rglob('*.dll'):
            add(path,f'{dest}/bin/{path.relative_to(sdk/"bin").as_posix()}',component=component)
        add(sdk/'bin/ThirdPartyLicenses.txt',f'{dest}/bin/ThirdPartyLicenses.txt',component=component)
        licenses(sdk,dest,component)
        features = ['nvargazeredirection','nvarlandmarkdetection','nvarfaceboxdetection'] if sdk == AR else VFX_FEATURES
        for feature in features:
            feature_root = sdk/'features'/feature
            for path in (feature_root/'bin').glob('*.dll'):
                add(path,f'{dest}/features/{feature}/bin/{path.name}',component=component)
            if not any((feature_root/'bin').glob('*.dll')):
                raise RuntimeError(f'Missing feature runtime: {feature}')
            licenses(feature_root,f'{dest}/features/{feature}',component)
        for path in (sdk/'bin/models').rglob('*'):
            if not path.is_file() or path.suffix.lower() not in ('.trtpkg','.nvf','.mtl'):
                continue
            architecture = next((gpu for gpu,(sm,_) in ARCHITECTURES.items() if f'_{sm}.' in path.name or f'_{sm}_' in path.name or f'-{sm}.' in path.name), None)
            if path.suffix == '.trtpkg' and not architecture:
                raise RuntimeError(f'Unclassified model architecture: {path.name}')
            add(path,f'{dest}/bin/models/{path.relative_to(sdk/"bin/models").as_posix()}',
                payload=f'models-{architecture}' if architecture else 'runtime',component=component)
    for path in (AFX/'bin').rglob('*.dll'):
        add(path,f'runtime/AudioFX/bin/{path.relative_to(AFX/"bin").as_posix()}',component='audio')
    licenses(AFX,'runtime/AudioFX','audio')
    for path in (AFX/'bin').rglob('*'):
        if path.is_file() and path.suffix.lower() in ('.txt','.pdf'):
            add(path,f'runtime/AudioFX/bin/{path.relative_to(AFX/"bin").as_posix()}',component='audio')
    for feature in AFX_FEATURES:
        base=AFX/'features'/feature
        add(base/'bin'/f'{feature}.dll',f'runtime/AudioFX/features/{feature}/bin/{feature}.dll',component='audio')
        licenses(base,f'runtime/AudioFX/features/{feature}','audio')
        for gpu,(_,architecture) in ARCHITECTURES.items():
            models=list((base/'models'/architecture).glob('*.trtpkg'))
            if not models: raise RuntimeError(f'Missing {feature} {architecture} models')
            for path in models:
                add(path,f'runtime/AudioFX/features/{feature}/models/{architecture}/{path.name}',f'models-{gpu}','audio')
    if runtime_only:
        return
    for target in VIDEO:
        add(ROOT/'build/bin'/f'4x4{target}.aex',f'plugins/video/4x4{target}.aex','plugins','video')
    for target in AUDIO:
        add(ROOT/'build/bin'/f'4x4{target}.aex',f'plugins/audio/4x4{target}.aex','plugins','audio')
    bundle=ROOT/'build/VST3/Release/4x4Tools Audio.vst3'
    for relative in ('Contents/x86_64-win/4x4Tools Audio.vst3','Contents/Resources/moduleinfo.json','PlugIn.ico','desktop.ini'):
        add(bundle/relative,f'plugins/vst3/4x4Tools Audio.vst3/{relative}','plugins','audio')
    for name,component in (('SupportCheck.exe','common'),('EyeContactCli.exe','video'),('VideoEffectsCli.exe','video'),('AudioEffectsCli.exe','audio')):
        add(ROOT/'build/bin'/name,f'Tools/{name}','plugins',component)
    for name in ('README.md','THIRD-PARTY-NOTICES.md','QUALITY-REPORT.md'):
        add(ROOT/name,name,'plugins')
    add(ROOT/'installer/uninstall-suite.ps1','uninstall-suite.ps1','plugins')
    add(ROOT/'resources/branding/4x4Tools.ico','4x4Tools.ico','plugins')
    add(ROOT/'third_party/vst3sdk/LICENSE.txt','Notices/Steinberg-VST3-LICENSE.txt','plugins','audio')


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--manifest-only',action='store_true')
    parser.add_argument('--runtime-only',action='store_true')
    args=parser.parse_args()
    collect(args.runtime_only)
    if len({f['path'].casefold() for f in FILES}) != len(FILES):
        raise RuntimeError('Duplicate package destination')
    for gpu,(sm,architecture) in ARCHITECTURES.items():
        for prefix in ('gazeredir_encoder_fp16','gazeredir_decoder_fp16','face_detection','faceland_126_maxbatch1_mode0'):
            if not any(f['path'].endswith(f'{prefix}_{sm}.engine.trtpkg') for f in FILES):
                raise RuntimeError(f'Missing Eye Contact model for {gpu}')
        for prefix in ('AIGS_288x512_', 'Denoise-', 'relight_'):
            if not any(f['payload']==f'models-{gpu}' and prefix in f['path'] for f in FILES):
                raise RuntimeError(f'Missing video model {prefix} for {gpu}')
    (OUT/'Data').mkdir(parents=True,exist_ok=True)
    payloads=[]
    # A content-list fingerprint lets repeated packaging reuse unchanged multi-
    # gigabyte archives while still hashing the finished archive for Setup.
    for key,blobs in sorted(INPUTS.items()):
        archive=OUT/'Data'/f'{key}.zip'
        content_hash=hashlib.sha256(('\n'.join(sorted(blobs))).encode()).hexdigest()
        cache=ROOT/'build'/f'payload-{key}.json'
        prior=json.loads(cache.read_text()) if cache.exists() else {}
        if args.manifest_only and (prior.get('content')!=content_hash or not archive.exists() or prior.get('bytes')!=archive.stat().st_size):
            raise RuntimeError(f'{key} inputs changed. Rebuild payloads instead of --manifest-only.')
        rebuilt=False
        if not args.manifest_only and (not archive.exists() or prior.get('content')!=content_hash or prior.get('bytes')!=archive.stat().st_size):
            print(f'Packing {key}: {len(blobs)} unique files...',flush=True)
            temporary=archive.with_suffix('.zip.partial')
            with zipfile.ZipFile(temporary,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=3,allowZip64=True) as z:
                for sha,path in sorted(blobs.items()):
                    z.write(path,sha)
            os.replace(temporary,archive)
            rebuilt=True
        if not archive.is_file(): raise RuntimeError(f'Missing {archive.name}')
        archive_sha=digest(archive)
        if not rebuilt and prior.get('sha256') and prior['sha256']!=archive_sha:
            raise RuntimeError(f'Cached payload was modified: {archive.name}. Rebuild that payload.')
        cache.write_text(json.dumps(dict(content=content_hash,bytes=archive.stat().st_size,sha256=archive_sha)),encoding='utf-8')
        payloads.append(dict(id=key,file=f'Data/{key}.zip',sha256=archive_sha,bytes=archive.stat().st_size))
    if args.runtime_only:
        print('Large runtime/model payloads are ready; final plugin packaging remains.',flush=True)
        return
    manifest=dict(schema=1,product='4x4Tools Broadcast Suite',version=VERSION,architectures=ARCHITECTURES,
                  payloads=payloads,files=sorted(FILES,key=lambda f:f['path']))
    (ROOT/'build/suite-manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    shutil.copy2(ROOT/'installer/START-HERE.txt',OUT/'START HERE.txt')
    shutil.copy2(ROOT/'QUALITY-REPORT.md',OUT/'Quality Report.md')
    expanded=sum(f['bytes'] for f in FILES);stored=sum(p['bytes'] for p in payloads)
    print(f'Packaged {len(FILES)} files; {expanded/2**30:.2f} GiB expanded, {stored/2**30:.2f} GiB compressed. Credentials excluded.',flush=True)


if __name__=='__main__': main()
