"""Run the encrypted installer from a folder containing a native System.dll.

NSIS's component macros extract that DLL before launching Windows PowerShell.
It must not become the C# compiler's framework reference. The entire real helper
stages a tiny authenticated fixture; Adobe and Program Files are never changed.
"""
from pathlib import Path
import hashlib
import hmac
import io
import json
import os
import shutil
import subprocess
import tempfile
import zipfile

from Crypto.Cipher import AES
from Crypto.Util.Padding import pad

ROOT = Path(__file__).resolve().parents[1]


def main():
    (ROOT / 'build').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='setup bootstrap ', dir=ROOT / 'build') as temp:
        folder = Path(temp)
        shutil.copyfile(Path(os.environ['WINDIR']) / 'System32/version.dll', folder / 'System.dll')
        content = b'Authenticated installer bootstrap fixture.\n'
        digest = hashlib.sha256(content).hexdigest()
        stream = io.BytesIO()
        with zipfile.ZipFile(stream, 'w', zipfile.ZIP_DEFLATED) as archive:
            archive.writestr(digest, content)
        keys = os.urandom(64)
        iv = os.urandom(16)
        envelope = b'4X4ENC01' + iv + AES.new(keys[:32], AES.MODE_CBC, iv).encrypt(pad(stream.getvalue(), 16))
        envelope += hmac.new(keys[32:], envelope, hashlib.sha256).digest()
        package = folder / 'Package'
        (package / 'Data').mkdir(parents=True)
        payload = package / 'Data/plugins.4x4data'
        payload.write_bytes(envelope)
        keyfile = folder / 'fixture-key.bin'
        keyfile.write_bytes(keys)
        manifest = dict(schema=1, product='4x4Tools Broadcast Suite', version='fixture',
                        distribution='end-user-encrypted',
                        releaseBase='https://github.com/Insider4444/4x4-Tools-Broadcast-Suite/releases/download/v0.2.0-runtime',
                        files=[dict(path='README.md', payload='plugins', component='common',
                                    sha256=digest, bytes=len(content))],
                        payloads=[dict(id='plugins', file='Data/plugins.4x4data', asset=payload.name,
                                       sha256=hashlib.sha256(envelope).hexdigest(), bytes=len(envelope))])
        manifest_path = package / 'manifest.json'
        manifest_path.write_text(json.dumps(manifest), encoding='utf-8')
        logfile = folder / 'setup.log'
        stage = folder / 'Stage'
        command = ['powershell.exe', '-NoLogo', '-NoProfile', '-NonInteractive',
                   '-ExecutionPolicy', 'Bypass', '-File', str(ROOT / 'installer/setup-launcher.ps1'),
                   '-PackageRoot', str(package), '-Manifest', str(manifest_path),
                   '-Architecture', '120', '-KeyFile', str(keyfile),
                   '-StageOnly', '-StageRoot', str(stage), '-LogFile', str(logfile)]
        result = subprocess.run(command, cwd=folder, capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        assert (stage / 'README.md').read_bytes() == content
        transcript = logfile.read_text(encoding='utf-8-sig')
        assert 'Starting 4x4Tools Setup...' in transcript and 'Stage verified and configured:' in transcript
        assert keys.hex() not in transcript
        # A corrupt archive must fail before staging and retain useful details.
        payload.write_bytes(b'x' + envelope[1:])
        stage_index = command.index('-StageRoot') + 1
        command[stage_index] = str(folder / 'Rejected')
        result = subprocess.run(command, cwd=folder, capture_output=True, text=True)
        assert result.returncode != 0 and not (folder / 'Rejected').exists()
        transcript = logfile.read_text(encoding='utf-8-sig')
        assert 'Setup stopped:' in transcript and 'could not be verified' in transcript
    print('PASS: native System.dll cannot shadow .NET; encrypted staging succeeds; failures keep a diagnostic log.')


if __name__ == '__main__':
    main()
