"""Build end-user release assets. SDK checkouts and credentials never enter Git.

The envelope key is generated per release and embedded only in Setup. It is
recoverable by someone who has Setup; encryption is packaging, not model DRM.
"""
from pathlib import Path
import argparse
import hashlib
import hmac
import json
import os
import shutil
import zipfile

from Crypto.Cipher import AES
from Crypto.Util.Padding import pad
import package_suite as suite

ROOT = suite.ROOT
OUT = ROOT / 'dist' / 'release'
LIMIT = 1700 * 1024 * 1024  # ZIP and envelope overhead stay below GitHub's 2 GiB asset limit.
BASE = f'https://github.com/Insider4444/4x4-Tools-Broadcast-Suite/releases/download/v{suite.VERSION}'


def encrypt(source, target, keys):
    iv = os.urandom(16)
    cipher = AES.new(keys[:32], AES.MODE_CBC, iv)
    mac = hmac.new(keys[32:], digestmod=hashlib.sha256)
    with source.open('rb') as inp, target.with_suffix('.partial').open('wb') as out:
        def write(value):
            mac.update(value)
            out.write(value)
        write(b'4X4ENC01' + iv)
        while True:
            block = inp.read(8 * 1024 * 1024)
            if len(block) < 8 * 1024 * 1024:
                write(cipher.encrypt(pad(block, AES.block_size)))
                break
            write(cipher.encrypt(block))
        out.write(mac.digest())
    os.replace(target.with_suffix('.partial'), target)


def paths():
    settings = ROOT / 'sdk-paths.local.json'
    saved = json.loads(settings.read_text(encoding='utf-8-sig')) if settings.exists() else {}
    for attr, name, env in [('AR', 'ar', 'AR_SDK_ROOT'), ('VFX', 'vfx', 'VFX_SDK_ROOT'), ('AFX', 'afx', 'AFX_SDK_ROOT')]:
        setattr(suite, attr, Path(os.environ.get(env) or saved.get(name) or getattr(suite, attr)))


def license_text():
    import pymupdf
    texts, seen = [], set()
    for file in suite.FILES:
        source = suite.INPUTS[file['payload']][file['sha256']]
        if source.suffix.lower() == '.pdf' and file['sha256'] not in seen:
            seen.add(file['sha256'])
            with pymupdf.open(source) as doc:
                texts.append('\n\n' + source.name + '\n' + '\n'.join(p.get_text() for p in doc))
    intro = (ROOT / 'installer/END-USER-TERMS.txt').read_text(encoding='utf-8')
    (ROOT / 'build/release-license.txt').write_text(intro + '\n' + '\n'.join(texts), encoding='utf-8-sig')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--refresh', action='store_true', help='Repack changed contents; an existing release key is retained.')
    args = parser.parse_args()
    paths()
    suite.collect()
    # Public source MIT license, NVIDIA model attribution and end-user terms.
    for source, destination in [('LICENSE', 'LICENSE'), ('installer/END-USER-TERMS.txt', 'END-USER-TERMS.txt'),
                                ('installer/NVIDIA-MODEL-NOTICE.txt', 'Notices/Notice.txt'),
                                ('docs/USER-GUIDE.md', 'USER-GUIDE.md')]:
        suite.add(ROOT / source, destination, 'plugins')
    if len({f['path'].casefold() for f in suite.FILES}) != len(suite.FILES):
        raise RuntimeError('Duplicate release path')
    for f in suite.FILES:
        p = Path(f['path'])
        if p.suffix.lower() in ('.h', '.hpp', '.lib', '.pdb', '.psd', '.key') or any(s.startswith('.env') for s in p.parts):
            raise RuntimeError('Development/private file rejected: ' + f['path'])
    OUT.mkdir(parents=True, exist_ok=True)
    keyfile = ROOT / 'build/release-key.bin'
    if not keyfile.exists():
        keyfile.write_bytes(os.urandom(64))
    keys = keyfile.read_bytes()
    if len(keys) != 64:
        raise RuntimeError('Invalid local release key')
    license_text()  # Resolve source licenses before payload IDs are partitioned.
    key_id = hashlib.sha256(keys).hexdigest()  # Cache identifier only; not included in the public manifest.
    groups = {}
    for f in suite.FILES:
        group = f['payload'] + '-' + f['component']
        groups.setdefault(group, {})[f['sha256']] = suite.INPUTS[f['payload']][f['sha256']]
    payloads, assignments = [], {}
    for group, blobs in sorted(groups.items()):
        parts, part, size = [], {}, 0
        for sha, path in sorted(blobs.items()):
            if path.stat().st_size > LIMIT:
                raise RuntimeError('Single file exceeds release asset limit: ' + path.name)
            if part and size + path.stat().st_size > LIMIT:
                parts.append(part); part = {}; size = 0
            part[sha] = path; size += path.stat().st_size
        if part:
            parts.append(part)
        for index, part in enumerate(parts, 1):
            payload_id = f'{group}-{index:02}'
            target = OUT / f'{payload_id}.4x4data'
            fingerprint = hashlib.sha256((key_id + '\n' + '\n'.join(sorted(part))).encode()).hexdigest()
            cache = ROOT / 'build' / f'release-{payload_id}.json'
            prior = json.loads(cache.read_text()) if cache.exists() else {}
            if not target.exists() or prior.get('content') != fingerprint:
                print('Packing encrypted release asset:', target.name, flush=True)
                plain = ROOT / 'build' / f'release-{payload_id}.zip'
                with zipfile.ZipFile(plain, 'w', zipfile.ZIP_DEFLATED, compresslevel=3, allowZip64=True) as z:
                    for sha, path in part.items():
                        z.write(path, sha)
                encrypt(plain, target, keys)
                plain.unlink()
            sha = suite.digest(target)
            if prior.get('content') == fingerprint and prior.get('sha256') != sha:
                raise RuntimeError('Cached encrypted asset was modified: ' + target.name)
            cache.write_text(json.dumps(dict(content=fingerprint, sha256=sha)), encoding='utf-8')
            if target.stat().st_size >= 2**31:
                raise RuntimeError('Release asset exceeds 2 GiB')
            payloads.append(dict(id=payload_id, file='Data/' + target.name, asset=target.name,
                                 sha256=sha, bytes=target.stat().st_size, encryption='aes256-cbc-hmac256'))
            for blob in part:
                assignments[(group, blob)] = payload_id
    for f in suite.FILES:
        f['payload'] = assignments[(f['payload'] + '-' + f['component'], f['sha256'])]
    manifest = dict(schema=1, product='4x4Tools Broadcast Suite', version=suite.VERSION,
                    distribution='end-user-encrypted', releaseBase=BASE, architectures=suite.ARCHITECTURES,
                    payloads=payloads, files=sorted(suite.FILES, key=lambda f: f['path']))
    content = json.dumps(manifest, indent=2)
    (ROOT / 'build/release-manifest.json').write_text(content, encoding='utf-8')
    (OUT / 'release-manifest.json').write_text(content, encoding='utf-8')
    shutil.copy2(ROOT / 'installer/START-HERE.txt', OUT / 'START-HERE.txt')
    print(f'{len(suite.FILES)} runtime/plugin/notice files in {len(payloads)} encrypted assets; no SDK headers, developer tools or credentials.', flush=True)


if __name__ == '__main__':
    main()
