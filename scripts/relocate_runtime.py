"""Same-repository support-storage relocation; never overwrite an asset."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import tempfile
import time
import urllib.request


def api(path):
    return json.loads(subprocess.check_output(['gh','api',path],text=True))


def main():
    repo=os.environ['GITHUB_REPOSITORY']
    if repo!='Insider4444/4x4-Tools-Broadcast-Suite':raise RuntimeError('Relocation is restricted to the existing application repository.')
    root=Path(__file__).resolve().parents[1]
    config=json.loads((root/'resources/runtime-relocation.json').read_text())
    release=next(r for r in api('repos/'+repo+'/releases') if r['tag_name']==config['tag'])
    with tempfile.TemporaryDirectory(prefix='4x4-runtime-') as folder:
        for item in config['assets']:
            name=item['asset']
            if not re.fullmatch(r'[A-Za-z0-9.\-]+\.4x4data',name):raise RuntimeError('Invalid asset name')
            existing={a['name']:a for a in api('repos/'+repo+'/releases/'+str(release['id']))['assets']}
            if name in existing:
                a=existing[name]
                if a['state']=='uploaded' and a['size']==item['bytes'] and a.get('digest')=='sha256:'+item['sha256']:
                    print('Already verified:',name,flush=True);continue
                raise RuntimeError('Destination differs; nothing was overwritten')
            target=Path(folder)/name
            for attempt in range(3):
                try:
                    request=urllib.request.Request(config['sourceBase']+'/'+name,headers={'User-Agent':'4x4Tools-storage-relocation'})
                    h=hashlib.sha256();size=0
                    with urllib.request.urlopen(request,timeout=90) as response,target.open('wb') as out:
                        while block:=response.read(8*1024*1024):
                            out.write(block);h.update(block);size+=len(block)
                    if size!=item['bytes'] or h.hexdigest()!=item['sha256']:raise RuntimeError('Source checksum mismatch')
                    break
                except Exception:
                    if attempt==2:raise
                    time.sleep(2)
            print('Uploading verified support file:',name,flush=True)
            subprocess.run(['gh','release','upload',config['tag'],'--repo',repo,str(target)],check=True)
            target.unlink()
            a=next(a for a in api('repos/'+repo+'/releases/'+str(release['id']))['assets'] if a['name']==name)
            if a['size']!=item['bytes'] or a.get('digest')!='sha256:'+item['sha256']:raise RuntimeError('Destination checksum mismatch')
    print('Existing public support files verified in the same repository.',flush=True)


if __name__=='__main__':main()
