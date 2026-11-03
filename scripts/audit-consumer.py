"""Verify the two-file public release and new installer-managed payloads."""
from pathlib import Path
import hashlib
import importlib.util
import json
import zipfile

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('release_audit',ROOT/'scripts/audit-release.py')
crypto=importlib.util.module_from_spec(spec);spec.loader.exec_module(crypto)


def main():
    base=ROOT/'dist/community-0.2.0';public=base/'public';support=base/'support'
    assert {p.name for p in public.iterdir()}=={'4x4Tools-Setup.exe','SHA256SUMS.txt'}
    sha,name=(public/'SHA256SUMS.txt').read_text().strip().split('  ',1)
    assert name=='4x4Tools-Setup.exe' and crypto.digest(public/name)==sha
    manifest=json.loads((ROOT/'build/consumer-manifest.json').read_text())
    catalog=json.loads((ROOT/'resources/runtime-catalog.json').read_text())
    assert manifest['version']=='0.2.0' and manifest['releaseBase']==catalog['releaseBase']
    old={f['path']:f for f in catalog['files']};new={f['path']:f for f in manifest['files']}
    assert len(new)==len(manifest['files']) and all(new[p]==f for p,f in old.items())
    old_payloads={p['id']:p for p in catalog['payloads']}
    assert all(p in manifest['payloads'] for p in old_payloads.values())
    for f in manifest['files']:
        path=Path(f['path'])
        assert not path.is_absolute() and '..' not in path.parts and ':' not in f['path']
        assert not any(p.lower() in {'include','headers','.env','.ngc'} for p in path.parts)
        assert path.suffix.lower() not in {'.h','.hpp','.lib','.pdb','.key','.pfx','.pem'}
    key=(ROOT/'build/release-key.bin').read_bytes()
    payloads=[p for p in manifest['payloads'] if p['id'] not in old_payloads]
    assert len(payloads)==3
    for p in payloads:
        source=support/p['asset']
        assert source.stat().st_size==p['bytes'] and crypto.digest(source)==p['sha256']
        plain=ROOT/'build'/('audit-'+p['id']+'.zip')
        try:
            crypto.decrypt(source,plain,key)
            expected={f['sha256']:f['bytes'] for f in manifest['files'] if f['payload']==p['id']}
            with zipfile.ZipFile(plain) as z:
                assert set(z.namelist())==set(expected) and len(z.namelist())==len(expected)
                for sha,size in expected.items():
                    value=z.read(sha);assert len(value)==size and hashlib.sha256(value).hexdigest()==sha
        finally:
            if plain.exists():plain.unlink()
    assert {p.name for p in support.iterdir()}=={'manifest-0.2.0.json'}|{p['asset'] for p in payloads}
    print('PASS: public release contains only Setup + checksum; 3 authenticated support packages; immutable runtime catalog; no SDK headers or credentials.')


if __name__=='__main__':main()
