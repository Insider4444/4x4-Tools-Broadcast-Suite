"""Fail closed on private dependencies/credentials in the Git publication set.

Inspects tracked + staged files, never prints matched secret values. Ignored
local .env values are compared privately when present; .env is never enumerated
as a publication input. This is not a substitute for reviewing the inventory.
"""
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
ALLOWED = {'.md', '.txt', '.json', '.yml', '.yaml', '.ps1', '.py', '.cpp', '.h', '.cs', '.nsi',
           '.r', '.rc', '.ico', '.bmp', '.svg', '.html', '.css', '.js', '.png'}
SPECIAL = {'LICENSE', '.gitignore', '.gitattributes'}
WEBSITE = {'website/dist/index.html','website/dist/styles.css','website/dist/app.js','website/dist/assets/mark.svg',
           'website/dist/assets/social-card.png','website/dist/assets/eye-contact-nvidia.png'}
PRIVATE = re.compile(r'(^|/)(build|dist|artifacts|third_party|\.tools|\.ngc|sdks?|dependencies)(/|$)|'
                     r'(^|/)(ARSDK_windows_|VFXSDK_windows_|[^/]*NVIDIA_AFX_SDK)|'
                     r'(^|/)(sdk-paths\.local\.json|release-key\.[^/]+|\.env[^/]*)(/|$)', re.I)
SECRETS = [re.compile(rb'nvapi-' + rb'[A-Za-z0-9_-]{25,}'),
           re.compile(rb'gh[pousr]_' + rb'[A-Za-z0-9]{30,}'),
           re.compile(rb'github_pat_' + rb'[A-Za-z0-9_]{40,}'),
           re.compile(rb'-----BEGIN ' + rb'(?:RSA |EC |OPENSSH )?PRIVATE KEY-----')]


def main():
    listing = subprocess.check_output(['git', 'ls-files', '-z', '--cached'], cwd=ROOT)
    names = sorted(set(p.decode('utf-8') for p in listing.split(b'\0') if p))
    if not names:
        raise SystemExit('No tracked/staged source to audit. Stage an explicit reviewed allowlist first.')
    local_values = []
    envfile = ROOT / '.env'
    if envfile.exists():
        for line in envfile.read_text(encoding='utf-8-sig').splitlines():
            if '=' in line and not line.lstrip().startswith('#'):
                value = line.split('=', 1)[1].strip().strip('"\'')
                if len(value) >= 12:
                    local_values.append(value.encode())
    errors = []
    for name in names:
        p = ROOT / name
        if name != '.env.example' and name not in WEBSITE and PRIVATE.search(name):
            errors.append((name, 'private/dependency path'))
        if p.suffix.lower() not in ALLOWED and p.name not in SPECIAL and name != '.env.example':
            errors.append((name, 'unexpected file type'))
        if p.is_symlink() or not p.is_file() or p.stat().st_size > 2 * 1024 * 1024:
            errors.append((name, 'link, missing file or oversized source asset')); continue
        content = subprocess.check_output(['git', 'show', ':' + name], cwd=ROOT)
        if any(pattern.search(content) for pattern in SECRETS) or any(value in content for value in local_values):
            errors.append((name, 'credential/private-key content'))
        if name == '.env.example':
            for line in content.decode().splitlines():
                if line.strip() and not line.lstrip().startswith('#') and line.strip() != 'NGC_CLI_API_KEY=':
                    errors.append((name, 'nonempty environment example'))
        if re.search(rb'[A-Za-z]:[\\/](?:CT\x20x\x204x4|Users[\\/]user[\\/])', content):
            errors.append((name, 'private developer workspace path'))
    if errors:
        for name, reason in errors:
            print('REJECT:', name, '(' + reason + ')')
        raise SystemExit(1)
    print(f'PASS: {len(names)} public source/doc/artwork files; no forbidden dependencies or recognized credentials.')


if __name__ == '__main__':
    main()
