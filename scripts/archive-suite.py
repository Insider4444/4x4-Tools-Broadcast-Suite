"""Wrap the already-compressed offline package in one portable ZIP64 file."""
from pathlib import Path
import hashlib
import os
import zipfile

ROOT=Path(__file__).resolve().parents[1]
FOLDER=ROOT/'dist/4x4Tools-Broadcast-Suite-0.2.0-preview.1'
ARCHIVE=FOLDER.parent/(FOLDER.name+'.zip')


def main():
    if not (FOLDER/'4x4Tools-Setup.exe').is_file() or not (FOLDER/'SHA256SUMS.txt').is_file():
        raise RuntimeError('Build the complete suite package first')
    partial=Path(str(ARCHIVE)+'.partial')
    with zipfile.ZipFile(partial,'w',compression=zipfile.ZIP_STORED,allowZip64=True) as z:
        for path in sorted(FOLDER.rglob('*')):
            if path.is_file():
                if path.name.startswith('.env') or path.suffix=='.partial':raise RuntimeError('Unexpected private/incomplete file')
                z.write(path,f'{FOLDER.name}/{path.relative_to(FOLDER).as_posix()}')
    os.replace(partial,ARCHIVE)
    h=hashlib.sha256()
    with ARCHIVE.open('rb') as stream:
        for block in iter(lambda:stream.read(8*1024*1024),b''):h.update(block)
    Path(str(ARCHIVE)+'.sha256').write_text(f'{h.hexdigest()}  {ARCHIVE.name}\n',encoding='ascii')
    print(f'Portable ZIP64 archive: {ARCHIVE} ({ARCHIVE.stat().st_size/2**30:.2f} GiB)',flush=True)


if __name__=='__main__':main()
