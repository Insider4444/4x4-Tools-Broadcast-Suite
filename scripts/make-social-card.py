"""Render the Eye Contact link preview with original gaze artwork and host badges."""
from pathlib import Path
import importlib.util
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]


def eye(image, center, looking_down=False):
    """Draw a gaze diagram; this is an illustration, not processed footage."""
    x, y = center
    width, height = 154, 66
    left, top = x - width // 2, y - height // 2
    points = []
    for step in range(81):
        t = step / 80
        points.append((left + width * t, top + height / 2 - height * 2 * t * (1 - t)))
    for step in range(80, -1, -1):
        t = step / 80
        points.append((left + width * t, top + height / 2 + height * 2 * t * (1 - t)))
    mask = Image.new('L', image.size)
    ImageDraw.Draw(mask).polygon(points, fill=255)
    detail = Image.new('RGB', image.size, '#e4eee8')
    draw = ImageDraw.Draw(detail)
    pupil_y = y + (19 if looking_down else 0)
    draw.ellipse((x - 27, pupil_y - 29, x + 27, pupil_y + 29), fill='#508f7f')
    draw.ellipse((x - 19, pupil_y - 22, x + 19, pupil_y + 22), fill='#74bba2')
    draw.ellipse((x - 11, pupil_y - 16, x + 11, pupil_y + 16), fill='#102b2a')
    draw.ellipse((x - 8, pupil_y - 12, x - 1, pupil_y - 5), fill='#e7fff0')
    image.paste(detail, mask=mask)
    ImageDraw.Draw(image).line(points + [points[0]], fill='#8dbbab', width=2)


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
    draw.text((917, 63), 'MADE FOR ADOBE', font=brand.font(17), fill='#a1b8b3')

    draw.text((55, 163), 'Eye Contact', font=brand.font(83, True), fill='#70e8cd')
    draw.text((60, 275), 'Look toward the camera.', font=brand.font(29), fill='#eef4ef')
    draw.text((60, 319), 'Inside your Adobe timeline.', font=brand.font(24), fill='#91aaa4')
    for left, short, name, accent in ((60, 'Ae', 'After Effects', '#c2b4ff'),
                                       (346, 'Pr', 'Premiere Pro', '#b4b5ff')):
        draw.rounded_rectangle((left, 398, left + 266, 480), radius=9, fill='#111b20', outline='#354447')
        draw.rounded_rectangle((left + 15, 413, left + 67, 465), radius=7, fill='#211a40', outline='#5f538c')
        draw.text((left + 22, 416), short, font=brand.font(27, True), fill=accent)
        draw.text((left + 81, 422), name, font=brand.font(23, True), fill='#e3ebe6')

    for top, title, corrected in ((164, 'LOOKING DOWN', False), (346, 'TOWARD THE CAMERA', True)):
        draw.rounded_rectangle((670, top, 1140, top + 151), radius=12,
                               fill='#13221f' if corrected else '#111b20',
                               outline='#477e6b' if corrected else '#324548', width=2)
        draw.text((692, top + 15), title, font=brand.font(15, True), fill='#91d5b9' if corrected else '#91a9ae')
        eye(image, (789, top + 96), looking_down=not corrected)
        eye(image, (1021, top + 96), looking_down=not corrected)
    draw.line((905, 320, 905, 337), fill='#70e8cd', width=3)
    draw.line((898, 331, 905, 338, 912, 331), fill='#70e8cd', width=3)
    draw.text((847, 510), 'GAZE ILLUSTRATION', font=brand.font(12), fill='#7f9d94')

    draw.line((60, 550, 1140, 550), fill='#283b37', width=2)
    draw.text((60, 575), '13 picture + voice effects  /  One installer', font=brand.font(21), fill='#b3c6be')
    draw.text((858, 577), 'NVIDIA RTX  /  Windows x64', font=brand.font(18), fill='#8fa9a3')
    destination = ROOT / 'website/dist/assets/social-card.png'
    image.save(destination, optimize=True)
    print('Generated original 1200 x 630 link-preview artwork.')


if __name__ == '__main__':
    main()
