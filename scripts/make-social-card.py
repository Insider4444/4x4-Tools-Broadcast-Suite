"""Render the Eye Contact link preview with the NVIDIA demo still and Adobe host badges."""
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
    logo = brand.mark(58)
    image.paste(logo, (60, 46), logo)
    draw.text((136, 43), '4x4-Tools', font=brand.font(31, True), fill='#f0f4ee')
    draw.text((138, 84), 'BROADCAST SUITE', font=brand.font(13), fill='#8fa6a9')
    draw.text((929, 63), 'EYE CONTACT', font=brand.font(17), fill='#a1b8b3')

    draw.text((55, 164), 'Eye Contact', font=brand.font(71, True), fill='#70e8cd')
    draw.text((60, 271), 'Inside your Adobe timeline.', font=brand.font(24), fill='#a9c1b8')
    for top, short, name, accent in ((348, 'Ae', 'After Effects', '#c2b4ff'),
                                     (432, 'Pr', 'Premiere Pro', '#b4b5ff')):
        draw.rounded_rectangle((60, top, 454, top + 66), radius=8, fill='#111b20', outline='#354447')
        draw.rounded_rectangle((74, top + 9, 122, top + 57), radius=6, fill='#211a40', outline='#5f538c')
        draw.text((80, top + 10), short, font=brand.font(27, True), fill=accent)
        draw.text((142, top + 13), name, font=brand.font(27, True), fill='#e3ebe6')

    # Preserve NVIDIA's complete OFF/ON comparison, including the source labels.
    # This frame is not a recording of the 4x4Tools Adobe plugin.
    still = Image.open(ROOT / 'website/dist/assets/eye-contact-nvidia.png').convert('RGB')
    if still.size != (630, 352):
        raise ValueError('The pinned NVIDIA comparison frame must remain 630 x 352.')
    image.paste(still, (510, 164))
    draw.text((510, 522), 'NVIDIA Broadcast demo © NVIDIA', font=brand.font(13), fill='#8fa9a3')

    draw.line((60, 550, 1140, 550), fill='#283b37', width=2)
    draw.text((60, 575), '13 picture + voice effects  /  One installer', font=brand.font(21), fill='#b3c6be')
    draw.text((858, 577), 'NVIDIA RTX  /  Windows x64', font=brand.font(18), fill='#8fa9a3')
    destination = ROOT / 'website/dist/assets/social-card.png'
    image.save(destination, optimize=True)
    print('Generated original 1200 x 630 link-preview artwork.')


if __name__ == '__main__':
    main()
