"""Cross-check Python envelopes with the exact Windows PowerShell 5.1 decoder."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from package_release import encrypt


def main():
    (ROOT / 'build').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='crypto-fixture-', dir=ROOT / 'build') as temp:
        folder = Path(temp); keys = os.urandom(64)
        (folder / 'key.bin').write_bytes(keys)
        (folder / 'wrong.bin').write_bytes(os.urandom(64))
        for length in (0, 1, 15, 16, 17, 1024 * 1024, 8 * 1024 * 1024 + 17):
            source = folder / f'{length}.raw'; source.write_bytes(os.urandom(length))
            encrypt(source, folder / f'{length}.enc', keys)
        bad = bytearray((folder / '17.enc').read_bytes()); bad[30] ^= 1
        (folder / 'corrupt.enc').write_bytes(bad)
        for suffix, content in [('header', b'WRONGHDR' + bytes(bad[8:])), ('truncated', bytes(bad[:35]))]:
            (folder / f'{suffix}.enc').write_bytes(content)
        runner = folder / 'test.ps1'
        runner.write_text(r'''$ErrorActionPreference='Stop'
$base=Split-Path -Parent $PSCommandPath
Add-Type -Path (Join-Path $base '..\..\installer\payload-crypto.cs')
$key=[IO.File]::ReadAllBytes((Join-Path $base 'key.bin'))
function Hash([string]$Path) {
    $algorithm=[Security.Cryptography.SHA256]::Create();$stream=[IO.File]::OpenRead($Path)
    try {return [BitConverter]::ToString($algorithm.ComputeHash($stream))}
    finally {$stream.Dispose();$algorithm.Dispose()}
}
foreach($source in Get-ChildItem -LiteralPath $base -Filter '*.raw') {
    $name=[IO.Path]::GetFileNameWithoutExtension($source.Name)
    $output=Join-Path $base ($name+'.decoded')
    [FourByFour.PayloadCrypto]::Decrypt((Join-Path $base ($name+'.enc')),$output,$key)
    if((Hash $source.FullName) -ne (Hash $output)) {throw 'Roundtrip mismatch'}
}
foreach($name in @('corrupt','header','truncated','wrongkey')) {
    $source=if($name -eq 'wrongkey'){Join-Path $base '17.enc'}else{Join-Path $base ($name+'.enc')}
    $selected=if($name -eq 'wrongkey'){[IO.File]::ReadAllBytes((Join-Path $base 'wrong.bin'))}else{$key}
    $output=Join-Path $base ($name+'.rejected');$caught=$false
    try {[FourByFour.PayloadCrypto]::Decrypt($source,$output,$selected)} catch {$caught=$true}
    if(!$caught -or (Test-Path -LiteralPath $output)) {throw 'Failed to reject invalid envelope cleanly'}
}
Write-Output 'PASS: seven boundary-size roundtrips; corrupt/header/truncation/wrong-key rejection; no rejected plaintext retained.'
''', encoding='utf-8-sig')
        result = subprocess.run(['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(runner)], cwd=ROOT, capture_output=True, text=True)
        print(result.stdout); print(result.stderr)
        if result.returncode:
            raise SystemExit(result.returncode)


if __name__ == '__main__':
    main()
