"""Audit only dist/release: explicit end-user inventory, sizes and digests.

--verify-payloads additionally authenticates/decrypts every envelope and checks
every decompressed content blob against the manifest. No secret values print.
"""
from pathlib import Path
import argparse
import hashlib
import hmac
import json
import re
import tempfile
import zipfile

from Crypto.Cipher import AES
from Crypto.Util.Padding import unpad
from package_suite import ROOT, digest


def decrypt(source, destination, keys):
    with source.open('rb') as inp:
        authenticated = source.stat().st_size - 32
        if authenticated < 40 or (authenticated - 24) % 16:
            raise RuntimeError('Bad encrypted length')
        mac = hmac.new(keys[32:], digestmod=hashlib.sha256)
        remaining = authenticated
        while remaining:
            block = inp.read(min(8 * 1024 * 1024, remaining))
            if not block:
                raise RuntimeError('Truncated payload')
            mac.update(block); remaining -= len(block)
        if not hmac.compare_digest(mac.digest(), inp.read(32)):
            raise RuntimeError('Authentication failed')
        inp.seek(0)
        if inp.read(8) != b'4X4ENC01':
            raise RuntimeError('Unexpected envelope')
        cipher = AES.new(keys[:32], AES.MODE_CBC, inp.read(16))
        remaining = authenticated - 24
        with destination.open('wb') as out:
            while remaining:
                block = inp.read(min(8 * 1024 * 1024, remaining)); remaining -= len(block)
                plain = cipher.decrypt(block)
                out.write(plain if remaining else unpad(plain, 16))


def main():
    parser = argparse.ArgumentParser(); parser.add_argument('--verify-payloads', action='store_true'); args = parser.parse_args()
    folder = ROOT / 'dist/release'
    manifest = json.loads((folder / 'release-manifest.json').read_text())
    if manifest['distribution'] != 'end-user-encrypted':
        raise RuntimeError('Not an end-user release')
    if any(x['bytes'] >= 2**31 for x in manifest['payloads']):
        raise RuntimeError('GitHub asset limit exceeded')
    inventory = {'4x4Tools-Setup.exe', 'release-manifest.json', 'SHA256SUMS.txt', 'START-HERE.txt'}
    inventory.update(x['asset'] for x in manifest['payloads'])
    if {p.name for p in folder.iterdir()} != inventory:
        raise RuntimeError('Unexpected or missing release file')
    for line in (folder / 'SHA256SUMS.txt').read_text().splitlines():
        sha, name = line.split('  ', 1)
        if name not in inventory or digest(folder / name) != sha:
            raise RuntimeError('Checksum list mismatch: ' + name)
    paths = set()
    for file in manifest['files']:
        name = file['path']; path = Path(name)
        if name.casefold() in paths or '..' in path.parts or path.is_absolute() or ':' in name:
            raise RuntimeError('Duplicate/unsafe runtime path')
        paths.add(name.casefold())
        if re.search(r'(^|/)(include|headers|samples?|\.env[^/]*|\.ngc|sdk-paths\.local\.json)(/|$)', name, re.I):
            raise RuntimeError('Private SDK path')
        if path.suffix.lower() not in {'.dll', '.aex', '.vst3', '.exe', '.json', '.ico', '.ini', '.md', '.txt', '.ps1', '.pdf', '.trtpkg', '.nvf', '.mtl', '.html'} and name != 'LICENSE':
            raise RuntimeError('Development/unexpected file: ' + name)
        if path.suffix.lower() == '.exe' and path.name not in {'SupportCheck.exe', 'EyeContactCli.exe', 'VideoEffectsCli.exe', 'AudioEffectsCli.exe'}:
            raise RuntimeError('Unexpected tool: ' + name)
        if path.suffix.lower() == '.ini' and path.name != 'desktop.ini':
            raise RuntimeError('Developer runtime INI')
        if not name.startswith(('runtime/', 'plugins/', 'Tools/', 'Notices/')) and name not in {'README.md','THIRD-PARTY-NOTICES.md','QUALITY-REPORT.md','LICENSE','END-USER-TERMS.txt','USER-GUIDE.md','uninstall-suite.ps1','4x4Tools.ico'}:
            raise RuntimeError('Unexpected destination')
    keys = (ROOT / 'build/release-key.bin').read_bytes() if args.verify_payloads else None
    for payload in manifest['payloads']:
        source = folder / payload['asset']
        if source.stat().st_size != payload['bytes'] or digest(source) != payload['sha256']:
            raise RuntimeError('Payload digest mismatch: ' + source.name)
        with source.open('rb') as inp:
            if inp.read(8) != b'4X4ENC01':
                raise RuntimeError('Unencrypted release asset')
        if args.verify_payloads:
            print('Authenticating and verifying:', source.name, flush=True)
            with tempfile.TemporaryDirectory(prefix='release-audit-', dir=ROOT / 'build') as temp:
                plain = Path(temp) / 'payload.zip'; decrypt(source, plain, keys)
                expected = {f['sha256']: f['bytes'] for f in manifest['files'] if f['payload'] == payload['id']}
                with zipfile.ZipFile(plain) as z:
                    if set(z.namelist()) != set(expected) or len(z.namelist()) != len(expected):
                        raise RuntimeError('Unexpected archive blob')
                    for name, size in expected.items():
                        entry = z.getinfo(name)
                        if entry.file_size != size:
                            raise RuntimeError('Blob size mismatch')
                        h = hashlib.sha256()
                        with z.open(entry) as inp:
                            for block in iter(lambda: inp.read(8 * 1024 * 1024), b''):
                                h.update(block)
                        if h.hexdigest() != name:
                            raise RuntimeError('Blob hash mismatch')
    print(f'PASS: {len(manifest["files"])} allowlisted installed files; {len(manifest["payloads"])} encrypted assets; source SDK/tool packages and credentials excluded.')


if __name__ == '__main__':
    main()
