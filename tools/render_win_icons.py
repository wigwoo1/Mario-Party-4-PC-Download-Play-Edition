"""Local diagnostic contact sheet of original C8 window icon bitmaps."""
import argparse
import struct
from PIL import Image, ImageDraw
from inspect_win_icons import Disc, inspect


def render(data, output):
    banks, patterns, count, _, bank_offset, pattern_offset, bitmap_offset = struct.unpack_from('>4h3I', data)
    sheet = Image.new('RGB', (800, ((count + 4) // 5) * 240), '#28313f')
    draw = ImageDraw.Draw(sheet)
    for index in range(count):
        bits, fmt, colors, width, height, size, palette, pixels = struct.unpack_from('>BBhhhIII', data, bitmap_offset + index * 20)
        if fmt != 3 or not 0 < width <= 256 or not 0 < height <= 256 or not 0 < colors <= 256:
            raise ValueError('Unsupported bitmap')
        if palette + colors * 2 > len(data) or pixels + size > len(data):
            raise ValueError('Invalid bitmap extent')
        rgba = []
        for color in struct.unpack_from('>' + 'H' * colors, data, palette):
            if color & 0x8000:
                rgba.append(tuple(((color >> shift) & 31) * 255 // 31 for shift in (10, 5, 0)) + (255,))
            else:
                rgba.append(tuple(((color >> shift) & 15) * 17 for shift in (8, 4, 0)) + (((color >> 12) & 7) * 255 // 7,))
        image = Image.new('RGBA', (width, height))
        for y in range(height):
            for x in range(width):
                offset = ((y // 4) * ((width + 7) // 8) + x // 8) * 32 + (y % 4) * 8 + x % 8
                if offset >= size:
                    raise ValueError('Truncated tiled bitmap')
                image.putpixel((x, y), rgba[data[pixels + offset]])
        left, top = (index % 5) * 160, (index // 5) * 240
        scale = min(4, 190 // height)
        image = image.resize((width * scale, height * scale), Image.Resampling.NEAREST)
        sheet.paste(image, (left + 12, top + 28), image)
        draw.text((left + 10, top + 8), f'Bitmap {index}', fill='white')
    sheet.save(output)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('disc')
    parser.add_argument('output')
    args = parser.parse_args()
    disc = Disc(args.disc)
    try:
        inspect(disc, lambda data: render(data, args.output))
    finally:
        disc.file.close()
