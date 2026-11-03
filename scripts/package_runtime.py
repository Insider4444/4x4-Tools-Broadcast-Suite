"""Stage an explicit runtime allowlist; credentials and SDK headers are excluded."""
from pathlib import Path
import hashlib
import json
import shutil

ROOT = Path(__file__).resolve().parent.parent
SDK = ROOT / 'ARSDK_windows_1.2.0.0' / 'ARSDK'
STAGE = ROOT / 'build' / 'package-stage'
BIN = ROOT / 'build' / 'bin'


def copy(source: Path, destination: Path) -> None:
    if not source.is_file():
        raise RuntimeError(f'Missing package input: {source}')
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not destination.exists() or source.stat().st_size != destination.stat().st_size or source.stat().st_mtime_ns != destination.stat().st_mtime_ns:
        shutil.copy2(source, destination)


def main() -> None:
    for sm in ('75', '86', '89', '100'):
        for prefix in ('gazeredir_encoder_fp16', 'gazeredir_decoder_fp16', 'face_and_landmark_detector_119_fp32', 'face_detection', 'faceland_126_maxbatch1_mode0', 'faceland_126_maxbatch1_mode1'):
            model = SDK / 'bin' / 'models' / f'{prefix}_{sm}.engine.trtpkg'
            if not model.is_file():
                raise RuntimeError(f'Missing model for supported GPU architecture: {model.name}')
    runtime = STAGE / 'runtime' / 'ARSDK'
    for name in ('4x4EyeContact.aex', 'SupportCheck.exe', 'EyeContactCli.exe'):
        copy(BIN / name, STAGE / name)
    copy(ROOT / 'docs' / 'EYE-CONTACT.md', STAGE / 'README.md')
    copy(ROOT / 'docs' / 'EYE-CONTACT-NOTICES.md', STAGE / 'THIRD-PARTY-NOTICES.md')
    for source in (SDK / 'bin').glob('*.dll'):
        copy(source, runtime / 'bin' / source.name)
    copy(SDK / 'bin' / 'ThirdPartyLicenses.txt', runtime / 'bin' / 'ThirdPartyLicenses.txt')
    for source in (SDK / 'bin' / 'models').iterdir():
        if source.is_file() and source.suffix in ('.trtpkg', '.nvf', '.mtl'):
            copy(source, runtime / 'bin' / 'models' / source.name)
    for source in (SDK / 'license').rglob('*'):
        if source.is_file():
            copy(source, runtime / 'license' / source.relative_to(SDK / 'license'))
    for feature, dll in (('nvargazeredirection', 'nvARGazeRedirection.dll'), ('nvarlandmarkdetection', 'nvARLandmarkDetection.dll'), ('nvarfaceboxdetection', 'nvARFaceBoxDetection.dll')):
        source_root = SDK / 'features' / feature
        copy(source_root / 'bin' / dll, runtime / 'features' / feature / 'bin' / dll)
        for source in (source_root / 'license').rglob('*'):
            if source.is_file():
                copy(source, runtime / 'features' / feature / 'license' / source.relative_to(source_root / 'license'))
    files = []
    for path in sorted(STAGE.rglob('*')):
        if not path.is_file() or path.name == 'runtime-manifest.json':
            continue
        if path.name.startswith('.env') or 'include' in path.parts:
            raise RuntimeError(f'Unexpected private/development input in package: {path}')
        digest = hashlib.sha256()
        with path.open('rb') as stream:
            for chunk in iter(lambda: stream.read(8 * 1024 * 1024), b''):
                digest.update(chunk)
        files.append({'path': path.relative_to(STAGE).as_posix(), 'bytes': path.stat().st_size, 'sha256': digest.hexdigest()})
    manifest = {'product': '4x4 Eye Contact', 'version': '0.1.0-dev.1', 'ar_sdk': '1.2.0.0',
                'gpu_model_architectures': ['75', '86', '89', '100 (Blackwell SM120)'], 'files': files}
    (STAGE / 'runtime-manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    print(f'Staged {len(files)} files, {sum(f["bytes"] for f in files) / 2**30:.2f} GiB. No credentials or SDK headers included.', flush=True)


if __name__ == '__main__':
    main()
