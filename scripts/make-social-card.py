"""Render original link-preview artwork using the canonical publisher mark."""
from pathlib import Path
import importlib.util
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]


def main():
    spec = importlib.util.spec_from_file_location('brand_art', ROOT / 'scripts/make-installer-art.py')
    brand = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(brand)
    image = Image.new('RGB', (1200, 630), '#080b0d')
    draw = ImageDraw.Draw(image)
    draw.rectangle((0, 0, 1199, 5), fill='#70e8cd')
    logo = brand.mark(70)
    image.paste(logo, (64, 55), logo)
    draw.text((154, 53), '4x4-Tools', font=brand.font(37, True), fill='#f0f4ee')
    draw.text((157, 105), 'BROADCAST SUITE', font=brand.font(15), fill='#8fa6a9')
    draw.text((59, 204), 'Bring your best.', font=brand.font(78, True), fill='#f0f4ee')
    draw.text((59, 301), 'Every frame.', font=brand.font(78, True), fill='#70e8cd')
    draw.line((857, 211, 857, 404), fill='#2b4540', width=2)
    draw.text((898, 203), '13', font=brand.font(124), fill='#70e8cd')
    draw.text((909, 362), 'PICTURE + VOICE', font=brand.font(16), fill='#a1bbb0')
    draw.text((909, 387), 'EFFECTS', font=brand.font(16), fill='#a1bbb0')
    draw.line((64, 466, 1136, 466), fill='#283b37', width=2)
    draw.text((64, 495), 'Premiere Pro + After Effects', font=brand.font(29, True), fill='#dce8df')
    draw.text((64, 546), 'NVIDIA RTX  /  Windows x64  /  One installer', font=brand.font(20), fill='#8fa9a3')
    draw.rounded_rectangle((886, 500, 1136, 558), radius=4, fill='#70e8cd')
    draw.text((913, 513), 'FREE COMMUNITY RELEASE', font=brand.font(15, True), fill='#173a2c')
    destination = ROOT / 'website/dist/assets/social-card.png'
    image.save(destination, optimize=True)
    print('Generated original 1200 x 630 link-preview artwork.')


if __name__ == '__main__':
    main()
