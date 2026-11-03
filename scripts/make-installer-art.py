"""Compile the suite's geometric brand artwork into Windows installer assets."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'resources' / 'branding'


def font(size, bold=False):
    return ImageFont.truetype('C:/Windows/Fonts/' + ('segoeuib.ttf' if bold else 'segoeui.ttf'), size)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    icon = Image.new('RGBA', (256, 256), (17, 25, 32, 255))
    d = ImageDraw.Draw(icon)
    d.rounded_rectangle((16, 16, 240, 240), radius=46, outline='#65e3b0', width=9)
    d.text((128, 119), '4×4', font=font(82, True), fill='white', anchor='mm')
    d.rounded_rectangle((68, 183, 188, 194), radius=5, fill='#65e3b0')
    icon.save(OUT / '4x4Tools.ico', sizes=[(s, s) for s in (16, 24, 32, 48, 64, 128, 256)])
    scale = 3
    welcome = Image.new('RGB', (164 * scale, 314 * scale), '#111920')
    d = ImageDraw.Draw(welcome)
    d.rounded_rectangle((24*scale, 31*scale, 140*scale, 113*scale), 12*scale, outline='#65e3b0', width=2*scale)
    d.text((82*scale, 71*scale), '4×4', font=font(37*scale, True), fill='white', anchor='mm')
    d.text((22*scale, 146*scale), '4x4Tools', font=font(19*scale, True), fill='white')
    d.text((22*scale, 179*scale), 'BROADCAST', font=font(12*scale, True), fill='#65e3b0')
    d.text((22*scale, 198*scale), 'SUITE', font=font(12*scale, True), fill='#65e3b0')
    d.text((22*scale, 242*scale), 'Create with clarity.', font=font(9*scale), fill='#c5d1da')
    d.text((22*scale, 260*scale), 'Made for Adobe.', font=font(9*scale), fill='#c5d1da')
    welcome.resize((164,314),Image.Resampling.LANCZOS).save(OUT/'Welcome.bmp')
    header = Image.new('RGB', (150*scale,57*scale), 'white')
    d = ImageDraw.Draw(header)
    d.rounded_rectangle((2*scale,8*scale,43*scale,49*scale),8*scale,fill='#111920')
    d.text((22.5*scale,27*scale),'4×4',font=font(13*scale,True),fill='#65e3b0',anchor='mm')
    d.text((53*scale,15*scale),'4x4Tools',font=font(14*scale,True),fill='#111920')
    d.text((54*scale,34*scale),'Broadcast Suite',font=font(8*scale),fill='#52616d')
    header.resize((150,57),Image.Resampling.LANCZOS).save(OUT/'Header.bmp')
    print('Built 4x4Tools installer icon and artwork.')


if __name__ == '__main__':
    main()
