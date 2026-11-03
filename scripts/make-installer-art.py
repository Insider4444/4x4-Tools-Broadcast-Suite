"""Generate shared 4x4-Tools branding from the DLSS-5 sixteen-tile SVG."""
from pathlib import Path
import re
import xml.etree.ElementTree as ET
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'resources/branding'
NS = '{http://www.w3.org/2000/svg}'


def font(size, bold=False):
    return ImageFont.truetype('C:/Windows/Fonts/' + ('segoeuib.ttf' if bold else 'segoeui.ttf'), size)


def mark(size):
    # Read the canonical vector geometry. Rasterize at 4x for small Windows sizes.
    root = ET.parse(OUT / '4x4Tools-mark.svg').getroot()
    side = size * 4
    result = Image.new('RGBA', (side, side))
    factor = side / 170
    stops = [tuple(bytes.fromhex(e.attrib['stop-color'][1:])) for e in root.iter(NS + 'stop')]
    for rect in root.iter(NS + 'rect'):
        x, y = float(rect.get('x', 0)), float(rect.get('y', 0))
        width, height = float(rect.get('width')), float(rect.get('height'))
        opacity = float(rect.get('opacity', 1))
        left, top = round(x * factor), round(y * factor)
        w, h = round(width * factor), round(height * factor)
        tile = Image.new('RGBA', (w, h))
        mask = Image.new('L', (w, h)); draw = ImageDraw.Draw(mask)
        draw.rounded_rectangle((0, 0, w-1, h-1), radius=round(7 * factor), fill=round(255 * opacity))
        pixels = tile.load()
        for py in range(h):
            for px in range(w):
                t = (px/max(1,w-1) + py/max(1,h-1)) / 2
                pixels[px,py] = tuple(round(a+(b-a)*t) for a,b in zip(stops[0], stops[1])) + (255,)
        tile.putalpha(mask); result.alpha_composite(tile, (left, top))
    return result.resize((size,size), Image.Resampling.LANCZOS)


def vectors():
    source = (OUT / '4x4Tools-mark.svg').read_text(encoding='utf-8')
    defs = re.search(r'<defs>.*?</defs>', source, re.S).group()
    tiles = re.search(r'<g fill=.*?</g>', source, re.S).group()
    for mode, color in [('dark','#eff5fc'), ('light','#132238')]:
        svg = ('<svg xmlns="http://www.w3.org/2000/svg" width="500" height="120" viewBox="0 0 500 120" role="img" aria-labelledby="title">'
               '<title id="title">4x4-Tools</title>' + defs +
               '<g transform="translate(8 16) scale(.51764706)">' + tiles + '</g>' +
               '<text x="120" y="79" fill="' + color + '" font-family="Segoe UI,Arial,sans-serif" font-size="53" font-weight="700" letter-spacing="-1">4x4-Tools</text></svg>')
        (OUT / ('4x4Tools-logo-' + mode + '.svg')).write_text(svg, encoding='utf-8')
    hero = ROOT / 'docs/assets/hero.svg'
    value = hero.read_text(encoding='utf-8')
    inline = '<!-- brand-mark:start -->\n <g transform="translate(55 62) scale(.24117647)">' + tiles.replace('accent', 'brand-accent') + '</g>\n <!-- brand-mark:end -->'
    hero.write_text(re.sub(r'<!-- brand-mark:start.*?<!-- brand-mark:end -->', lambda _: inline, value, flags=re.S), encoding='utf-8')


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    vectors()
    icon = Image.new('RGBA', (256,256)); d=ImageDraw.Draw(icon)
    d.rounded_rectangle((0,0,255,255), radius=40, fill='#111f2c')
    icon.alpha_composite(mark(200), (28,28))
    icon.save(OUT/'4x4Tools.ico', sizes=[(s,s) for s in (16,24,32,48,64,128,256)])
    scale=3
    welcome=Image.new('RGB',(164*scale,314*scale),'#111f2c'); d=ImageDraw.Draw(welcome)
    welcome.paste(mark(105*scale),(29*scale,34*scale),mark(105*scale))
    d.text((17*scale,157*scale),'4x4-Tools',font=font(20*scale,True),fill='white')
    d.text((18*scale,192*scale),'BROADCAST',font=font(12*scale,True),fill='#62edcf')
    d.text((18*scale,211*scale),'SUITE',font=font(12*scale,True),fill='#62edcf')
    d.text((18*scale,254*scale),'Create with clarity.',font=font(9*scale),fill='#c5d8e2')
    d.text((18*scale,272*scale),'Made for Adobe.',font=font(9*scale),fill='#c5d8e2')
    welcome=welcome.resize((164,314),Image.Resampling.LANCZOS);welcome.save(OUT/'Welcome.bmp')
    header=Image.new('RGB',(150*scale,57*scale),'white');d=ImageDraw.Draw(header)
    logo=mark(35*scale);header.paste(logo,(3*scale,11*scale),logo)
    d.text((48*scale,13*scale),'4x4-Tools',font=font(14*scale,True),fill='#132238')
    d.text((49*scale,34*scale),'Broadcast Suite',font=font(8*scale),fill='#52616d')
    header=header.resize((150,57),Image.Resampling.LANCZOS);header.save(OUT/'Header.bmp')
    sheet=Image.new('RGB',(900,420),'#111f2c');d=ImageDraw.Draw(sheet)
    sheet.paste(welcome,(25,65));sheet.paste(header,(225,65))
    sheet.paste(icon,(590,90),icon)
    d.text((25,15),'Shared 4x4-Tools brand / sixteen filled tiles',font=font(24,True),fill='white')
    d.text((225,150),'Same mark as DLSS-5',font=font(21,True),fill='#62edcf')
    d.text((225,190),'Installer + Windows icon',font=font(18),fill='#c5d8e2')
    (ROOT/'build').mkdir(exist_ok=True);sheet.save(ROOT/'build/brand-preview.png')
    print('Generated standard 4x4-Tools sixteen-tile brand assets.')


if __name__=='__main__': main()
