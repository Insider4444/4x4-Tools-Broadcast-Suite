"""One-file public download, with immutable installer-managed support assets.

Reuses the version-pinned application runtime catalog; emits only new own-code
payloads. It never repacks or changes already published models/libraries.
The existing matching packaging key and your own SDK licenses are required.
"""
from pathlib import Path
import hashlib
import json
import os
import zipfile

from package_release import encrypt, paths
import package_suite as suite

ROOT=suite.ROOT
VERSION='0.2.0'
OUT=ROOT/'dist/community-0.2.0'
PUBLIC=OUT/'public'
SUPPORT=OUT/'support'


def main():
    paths()
    catalog=json.loads((ROOT/'resources/runtime-catalog.json').read_text())
    PUBLIC.mkdir(parents=True,exist_ok=True);SUPPORT.mkdir(parents=True,exist_ok=True)
    key=(ROOT/'build/release-key.bin').read_bytes()
    if len(key)!=64:raise RuntimeError('The matching private packaging key is required.')
    for target in suite.VIDEO:
        suite.add(ROOT/'build/bin'/f'4x4{target}.aex',f'plugins/video/4x4{target}.aex','plugins','video')
    for target in suite.AUDIO:
        suite.add(ROOT/'build/bin'/f'4x4{target}.aex',f'plugins/audio/4x4{target}.aex','plugins','audio')
    bundle=ROOT/'build/VST3/Release/4x4Tools Audio.vst3'
    for rel in ['Contents/x86_64-win/4x4Tools Audio.vst3','Contents/Resources/moduleinfo.json','PlugIn.ico','desktop.ini']:
        source=ROOT/'resources/branding/4x4Tools.ico' if rel=='PlugIn.ico' else bundle/rel
        suite.add(source,f'plugins/vst3/4x4Tools Audio.vst3/{rel}','plugins','audio')
    for name,component in [('SupportCheck.exe','common'),('EyeContactCli.exe','video'),('VideoEffectsCli.exe','video'),('AudioEffectsCli.exe','audio')]:
        suite.add(ROOT/'build/bin'/name,f'Tools/{name}','plugins',component)
    for name in ['README.md','THIRD-PARTY-NOTICES.md','QUALITY-REPORT.md','LICENSE']:
        suite.add(ROOT/name,name,'plugins')
    for source,dest,component in [('docs/USER-GUIDE.md','USER-GUIDE.md','common'),('installer/END-USER-TERMS.txt','END-USER-TERMS.txt','common'),
        ('installer/NVIDIA-MODEL-NOTICE.txt','Notices/Notice.txt','common'),('installer/uninstall-suite.ps1','uninstall-suite.ps1','common'),
        ('resources/branding/4x4Tools.ico','4x4Tools.ico','common'),('third_party/vst3sdk/LICENSE.txt','Notices/Steinberg-VST3-LICENSE.txt','audio')]:
        suite.add(ROOT/source,dest,'plugins',component)
    payloads=[]
    for component in ['common','video','audio']:
        selected=[f for f in suite.FILES if f['component']==component]
        blobs={f['sha256']:suite.INPUTS['plugins'][f['sha256']] for f in selected}
        identity=f'plugins-{component}-v{VERSION}'
        plain=ROOT/'build'/f'{identity}.zip';target=SUPPORT/(identity+'.4x4data')
        fingerprint=hashlib.sha256((hashlib.sha256(key).hexdigest()+'\n'+'\n'.join(sorted(blobs))).encode()).hexdigest()
        cache=ROOT/'build'/f'consumer-{identity}.json'
        prior=json.loads(cache.read_text()) if cache.exists() else {}
        if not target.exists() or prior.get('content')!=fingerprint:
            with zipfile.ZipFile(plain,'w',zipfile.ZIP_DEFLATED,compresslevel=3,allowZip64=True) as z:
                for sha,source in sorted(blobs.items()):z.write(source,sha)
            encrypt(plain,target,key);plain.unlink()
        sha=suite.digest(target)
        if prior.get('content')==fingerprint and prior.get('sha256')!=sha:raise RuntimeError('Cached support payload was modified.')
        cache.write_text(json.dumps(dict(content=fingerprint,sha256=sha)),encoding='utf-8')
        for f in selected:f['payload']=identity
        payloads.append(dict(id=identity,file='Data/'+target.name,asset=target.name,sha256=suite.digest(target),bytes=target.stat().st_size,encryption='aes256-cbc-hmac256'))
    files=sorted(catalog['files']+suite.FILES,key=lambda f:f['path'])
    if len({f['path'].casefold() for f in files})!=len(files):raise RuntimeError('Duplicate consumer package destination')
    manifest=dict(schema=1,product=catalog['product'],version=VERSION,distribution='end-user-encrypted',releaseBase=catalog['releaseBase'],
                  architectures=catalog['architectures'],payloads=catalog['payloads']+payloads,files=files)
    (ROOT/'build/consumer-manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    (SUPPORT/'manifest-0.2.0.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    # Full supplied license agreements stay in the installer and installed runtime.
    import pymupdf
    texts=[];seen=set()
    roots={'ARSDK':suite.AR,'VideoFX':suite.VFX,'AudioFX':suite.AFX}
    for file in catalog['files']:
        if not file['path'].lower().endswith('.pdf') or file['sha256'] in seen:continue
        parts=Path(file['path']).parts;source=roots[parts[1]].joinpath(*parts[2:])
        if suite.digest(source)!=file['sha256']:raise RuntimeError('Your SDK license does not match the pinned runtime catalog.')
        seen.add(file['sha256'])
        with pymupdf.open(source) as doc:texts.append('\n\n'+source.name+'\n'+'\n'.join(p.get_text() for p in doc))
    (ROOT/'build/consumer-license.txt').write_text((ROOT/'installer/END-USER-TERMS.txt').read_text(encoding='utf-8')+'\n'.join(texts),encoding='utf-8-sig')
    print(f'{len(files)} installed files; {len(payloads)} new own-code support assets; published runtime/models unchanged.')


if __name__=='__main__':main()
