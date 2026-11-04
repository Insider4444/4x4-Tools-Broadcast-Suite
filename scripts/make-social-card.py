"""Import an explicitly selected hero capture as the social card, unchanged."""
from pathlib import Path
import argparse
import hashlib
import re
import shutil
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True, help='Approved PNG hero capture')
    source = parser.parse_args().source.resolve()
    with Image.open(source) as image:
        if image.format != 'PNG':
            raise ValueError('Select the original PNG capture.')
        width, height = image.size
        image.verify()
    destination = ROOT / 'website/dist/assets/social-card.png'
    if source != destination:
        shutil.copyfile(source, destination)
    fingerprint = hashlib.sha256(destination.read_bytes()).hexdigest()[:12]
    path = ROOT / 'website/dist/index.html'
    html = path.read_text(encoding='utf-8')
    html = re.sub(r'social-card\.png\?v=[^"\s]+', 'social-card.png?v=hero-' + fingerprint, html)
    for dimension, value in [('width', width), ('height', height)]:
        html = re.sub(r'(<meta property="og:image:' + dimension + r'" content=")[^"]+',
                      lambda match: match.group(1) + str(value), html)
    path.write_text(html, encoding='utf-8')
    print('Imported the original hero capture without editing; refreshed OG/X dimensions and image version.')


if __name__ == '__main__':
    main()
